#include "common.h"
#include "c-icap.h"
#include "commands.h"
#include "debug.h"
#include "cache.h"
#include "module.h"
#include "proc_mutex.h"
#include "shared_mem.h"
#include <assert.h>

static int init_shared_cache(struct ci_server_conf *server_conf);
static void release_shared_cache();

CI_DECLARE_MOD_DATA common_module_t module = {
    "shared_cache",
    init_shared_cache,
    NULL,
    release_shared_cache,
    NULL,
};

struct ci_cache_type ci_shared_cache;
static int init_shared_cache(struct ci_server_conf *server_conf)
{
    ci_cache_type_register(&ci_shared_cache);
    return 1;
}

static void release_shared_cache()
{
}

int ci_shared_cache_init(struct ci_cache *cache, const char *name);
const void *ci_shared_cache_search(struct ci_cache *cache, const void *key, void **val, void *data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *data));
int ci_shared_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size));
void ci_shared_cache_destroy(struct ci_cache *cache);

struct ci_cache_type ci_shared_cache = {
    ci_shared_cache_init,
    ci_shared_cache_search,
    ci_shared_cache_update,
    ci_shared_cache_destroy,
    "shared"
};

/*Should be power of 2 and equal or less than 64*/
#define CACHE_PAGES 4

struct shared_cache_stats {
    int cache_users;
    struct page_stats {
        int64_t hits;
        int64_t searches;
        int64_t updates;
        int64_t update_hits;
    } page[CACHE_PAGES];
};

struct shared_cache_data {
    void *mem_ptr;
    void *slots;
    ci_shared_mem_id_t id;
    size_t max_hash;
    size_t entry_size;
    size_t shared_mem_size;
    int entries;
    int pages;
    int page_size;
    int page_shift_op;
    struct shared_cache_stats *stats;
    ci_proc_mutex_t cache_mutex;
    ci_proc_mutex_t mutex[CACHE_PAGES];
};

struct shared_cache_slot {
    unsigned int hash;
    time_t expires;
    size_t key_size;
    size_t value_size;
    unsigned char bytes[];
};

unsigned int
ci_hash_compute2(unsigned long hash_max_value, const void *data, unsigned int len)
{
    const unsigned char *s = (const unsigned char *)(data);
    unsigned int n = 0;
    unsigned int j = 0;
    unsigned int i = 0;
    while ((s - (const unsigned char *)data) < len) {
        ++j;
        n ^= 271 * *s;
        ++s;
    }
    i = n ^ (j * 271);
    return i % hash_max_value;
}

const char *ci_shared_mem_print_id(char *buf, size_t size, ci_shared_mem_id_t *id)
{
    if (buf) {
        if (id->scheme)
            id->scheme->shared_mem_print_info(id, buf, size);
        else
            *buf = '\0';

    }
    return buf;
}

void command_attach_shared_mem(const char *name, int type, void *data)
{
    char buf[128];
    struct shared_cache_data *shared_cache = (struct shared_cache_data *)data;
    shared_cache->mem_ptr = ci_shared_mem_attach(&shared_cache->id);
    shared_cache->stats = (struct shared_cache_stats *)shared_cache->mem_ptr;
    shared_cache->slots = (void *)(shared_cache->mem_ptr + sizeof(struct shared_cache_stats));
    ci_debug_printf(3, "Shared cache id:'%s' attached on address %p\n", ci_shared_mem_print_id(buf, sizeof(buf), &shared_cache->id), shared_cache->mem_ptr);
    ci_proc_mutex_lock(&(shared_cache->cache_mutex));
    ++shared_cache->stats->cache_users;
    ci_proc_mutex_unlock(&(shared_cache->cache_mutex));
}

int ci_shared_cache_init(struct ci_cache *cache, const char *name)
{
    unsigned int next_hash = 63;
    unsigned int final_max_hash = 63;
    int i;
    struct shared_cache_data *data;
    data = (struct shared_cache_data *)malloc(sizeof(struct shared_cache_data));
    data->entry_size = _CI_ALIGN(cache->max_object_size > 0 ? cache->max_object_size : 1);
    data->entries = _CI_ALIGN(cache->mem_size) / data->entry_size;

    while (next_hash < data->entries) {
        final_max_hash = next_hash;
        next_hash++;
        next_hash = (next_hash << 1) -1;
    }

    data->max_hash = final_max_hash;
    data->entries = final_max_hash + 1;
    data->shared_mem_size = sizeof(struct shared_cache_stats) + data->entries * data->entry_size;

    data->mem_ptr = ci_shared_mem_create(&data->id, name, data->shared_mem_size);
    if (!data->mem_ptr) {
        free(data);
        ci_debug_printf(1, "Error allocating shared mem for %s cache\n", name);
        return 0;
    }
    data->stats = (struct shared_cache_stats *)data->mem_ptr;
    data->slots = data->mem_ptr + sizeof(struct shared_cache_stats);
    memset(data->stats, 0, sizeof(struct shared_cache_stats));
    data->stats->cache_users = 1;

    /*TODO: check for error*/
    for (i = 0; i < CACHE_PAGES; ++i) {
        ci_proc_mutex_init(&(data->mutex[i]), name);
    }
    ci_proc_mutex_init(&(data->cache_mutex), name);

    data->page_size = data->entries / CACHE_PAGES;
    /* CACHE_PAGES can not be bigger than 64, the minimum entries value*/
    assert(data->entries % data->page_size == 0);
    data->pages = CACHE_PAGES;
    /* The pages and page_size should be a power of 2*/
    assert((data->pages & (data->pages - 1)) == 0);
    assert((data->page_size & (data->page_size - 1)) == 0);
    for (data->page_shift_op = 0; ((data->page_size >> data->page_shift_op) & 0x1) ==0 && data->page_shift_op < 64; ++data->page_shift_op );
    assert(data->page_shift_op < 64);

    ci_debug_printf(1, "Shared mem %s created\nMax shared memory: %u (of the %u requested), max entry size: %u, maximum entries: %u\n", name, (unsigned int)data->shared_mem_size, (unsigned int)cache->mem_size, (unsigned int)data->entry_size, data->entries);

    cache->cache_data = data;
    ci_command_register_action("shared_cache_attach_cmd", CHILD_START_CMD, data, command_attach_shared_mem);
    return 1;
}

int rw_lock_page(struct shared_cache_data *cache_data, int pos)
{
    ci_proc_mutex_lock(&cache_data->mutex[pos >> cache_data->page_shift_op]);
    return 1;
}

int rd_lock_page(struct shared_cache_data *cache_data, int pos)
{
    ci_proc_mutex_lock(&cache_data->mutex[pos >> cache_data->page_shift_op]);
    return 1;
}

void unlock_page(struct shared_cache_data *cache_data, int pos)
{
    ci_proc_mutex_unlock(&cache_data->mutex[pos >> cache_data->page_shift_op]);
}

time_t ci_internal_time()
{
    return time(NULL);
}

const void *ci_shared_cache_search(struct ci_cache *cache, const void *key, void **val, void *user_data, void *(*dup_from_cache)(const void *stored_val, size_t stored_val_size, void *user_data))
{
    time_t current_time;
    const void *cache_key, *cache_val;
    struct shared_cache_data *cache_data = cache->cache_data;
    unsigned int hash = ci_hash_compute(cache_data->max_hash, key, cache->key_ops->size(key));
    *val = NULL;
    if (hash >= cache_data->entries)
        hash = cache_data->entries -1;

    if (!rd_lock_page(cache_data, hash))
        return NULL;
    unsigned int page = (hash >> cache_data->page_shift_op);
    ++cache_data->stats->page[page].searches;
    unsigned int pos;
    int done;
    for (pos = hash, done = 0, cache_key = NULL;
            !cache_key && !done && ((pos >> cache_data->page_shift_op) == page);
            ++pos) {
        struct shared_cache_slot *slot = cache_data->slots + (pos * cache_data->entry_size);
        cache_key = (const void *)slot->bytes;
        cache_val = (const void *)(&slot->bytes[slot->key_size + 1]);

        if (slot->hash != hash) {
            cache_key = NULL;
            done = 1;
        } else if (cache->key_ops->compare(cache_key, key) == 0) {
            current_time = ci_internal_time();
            if (slot->expires < current_time)
                cache_key = NULL;
            else if (slot->value_size) {
                if (dup_from_cache)
                    *val = (*dup_from_cache)(cache_val, slot->value_size, user_data);
                else {
                    if ((*val = ci_buffer_alloc(slot->value_size)))
                        memcpy(*val, cache_val, slot->value_size);
                }
            }
        } else
            cache_key = NULL;
    }

    if (cache_key)
        ++cache_data->stats->page[page].hits;

    unlock_page(cache_data, hash);
    return cache_key;
}

int ci_shared_cache_update(struct ci_cache *cache, const void *key, const void *val, size_t val_size, void *(*copy_to_cache)(void *buf, const void *val, size_t buf_size))
{
    time_t expire_time, current_time;
    void *cache_key, *cache_val;
    size_t key_size;
    int ret, can_updated;
    struct shared_cache_data *cache_data = cache->cache_data;
    key_size = cache->key_ops->size(key);
    if ((key_size + val_size + sizeof(struct shared_cache_slot)) > cache_data->entry_size) {
        /*Does not fit to a cache_data slot.*/
        return 0;
    }

    unsigned int hash = ci_hash_compute(cache_data->max_hash, key, key_size);
    if (hash >= cache_data->entries)
        hash = cache_data->entries -1;

    current_time = ci_internal_time();
    expire_time = current_time + cache->ttl;

    if (!rw_lock_page(cache_data, hash))
        return 0; /*not able to obtain a rw lock*/

    unsigned int page = (hash >> cache_data->page_shift_op);
    ++cache_data->stats->page[page].updates;

    unsigned int pos;
    int done;
    for (pos = hash, ret = 0, done = 0;
            ret == 0 && !done && ((hash >> cache_data->page_shift_op) == (pos >> cache_data->page_shift_op));
            ++pos) {
        struct shared_cache_slot *slot = cache_data->slots + (pos * cache_data->entry_size);

        cache_key = (void *)slot->bytes;
        can_updated = 0;
        if (slot->hash < hash) {
            can_updated = 1;
        } else if (cache->key_ops->compare(cache_key, key) == 0) {
            /*we are updating key with a new value*/
            can_updated = 1;
        } else if (slot->expires < current_time + cache->ttl) {
            can_updated = 1;
        } else if (pos == hash && slot->expires < (current_time + (cache->ttl / 2))) {
            /*entries on pos == hash which are near to expire*/
            can_updated = 1;
        } else if (pos != hash && slot->hash == pos) {
            /*entry is not expired, and it is not on a continues block we can use  */
            done = 1;
        }
        if (can_updated) {
            slot->hash = pos;
            slot->expires = expire_time;
            slot->key_size = key_size;
            slot->value_size = val_size;
            memcpy(cache_key, key, key_size);
            cache_val = (void *)(&slot->bytes[slot->key_size + 1]);
            if (copy_to_cache)
                copy_to_cache(cache_val, val, slot->value_size);
            else
                memcpy(cache_val, val, slot->value_size);
            ret = 1;
            ++cache_data->stats->page[page].update_hits;
        } else
            ret = 0;
    }

    unlock_page(cache_data, hash);
    return ret;
}

void ci_shared_cache_destroy(struct ci_cache *cache)
{
    int i, users;
    uint64_t updates, update_hits, searches, hits;
    struct shared_cache_data *data = cache->cache_data;
    ci_proc_mutex_lock(&data->cache_mutex);
    users = --data->stats->cache_users;
    ci_proc_mutex_unlock(&data->cache_mutex);
    if (users == 0) {
        updates = update_hits = searches = hits = 0;
        for (i = 0; i < CACHE_PAGES; ++i) {
            updates += data->stats->page[i].updates;
            update_hits += data->stats->page[i].update_hits;
            searches += data->stats->page[i].searches;
            hits += data->stats->page[i].hits;
        }
        ci_debug_printf(3, "Last user, the cache will be destroyed\n");
        ci_debug_printf(3, "Cache updates: %" PRIu64 ", update hits:%" PRIu64 ", searches: %" PRIu64 ", hits: %" PRIu64 "\n",
                        updates, update_hits, searches, hits
                       );
        ci_shared_mem_destroy(&data->id);
        ci_proc_mutex_destroy(&data->cache_mutex);
        for (i = 0; i < CACHE_PAGES; ++i) {
            ci_proc_mutex_destroy(&data->mutex[i]);
        }
    } else
        ci_shared_mem_detach(&data->id);
}
