#include <stdio.h>
#include <stdlib.h>
#include <time.h>


struct ci_cache_entry {
   unsigned int hash;
   time_t time;
   void *key;
   int keylen;
   void *val;
   struct ci_cache_entry *qnext;
   struct ci_cache_entry *hnext;
};


#define CI_CACHE_CONT_MEMBLOCK       0x1
#define CI_CACHE_ALLOC_MEMOBJECTS    0x2

struct cache_table { 
    struct ci_cache_entry *first_queue_entry; 
    struct ci_cache_entry *last_queue_entry;
    struct ci_cache_entry **hash_table;
    time_t ttl;
    unsigned int cache_size;
    unsigned int hash_table_size;
    unsigned int flags;
    int (*compare)(void *key1, int keylen1, void *key2, int keylen2);
    void (*release) (void *key,void *val);
};

time_t ci_internal_time()
{
    return time(NULL);
}


unsigned int compute_hash(struct ci_cache_table *cache, void *key, int len){
    unsigned long hash = 5381;
    unsigned char *s = key;
    int i;

    if(len) {
	for (i=0; i<len; i++,s++)
	    hash = ((hash << 5) + hash) + *s;
    }
    else
	while (*s) {
	    hash = ((hash << 5) + hash) + *s; /* hash * 33 + current char */
	    s++;
	}
    
    if(hash==0) hash++;
    hash = hash & cache->hash_table_size; /*Keep only the bits we need*/
    return hash;
}

struct ci_cache_table *ci_cache_build(unsigned int hash_size,
				unsigned int cache_size, 
				int ttl,
				unsigned int flags,
				int (*compare)(void *, int ,void *, int ),
				void (*release) (void *key,void *val)) {
   struct ci_cache_table *cache;
   int i;
   unsigned int new_hash_size;
   cache = malloc(sizeof(struct ci_cache_table));
   cache->flags = flags;

    if(cache_size) {
	if(cache->flags &  CI_CACHE_CONT_MEMBLOCK) {
	    /*Allocating continue memory block for cache, maybe we use it in a 
	      shared memory...*/ 
	    cache->first_queue_entry = calloc(cache_size,sizeof(struct ci_cache_entry));
	    for (i=0; i<cache_size-1; i++)
		cache->first_queue_entry[i].qnext = &(cache->first_queue_entry[i+1]);
	    cache->first_queue_entry[cache_size-1].qnext = NULL;      
	    cache->last_queue_entry=&(cache->first_queue_entry[cache_size-1]);
	}
	else {
	    cache->first_queue_entry = calloc(1,sizeof(struct ci_cache_entry)); 
	    cache->last_queue_entry = cache->first_queue_entry;
	    for (i=0; i < cache_size-1; i++) {
		cache->last_queue_entry->qnext=calloc(1,sizeof(struct ci_cache_entry));
		cache->last_queue_entry=cache->last_queue_entry->qnext;
	    }
	}
   }
   else {
        cache->first_queue_entry = NULL;
        cache->last_queue_entry = NULL;
   }
   cache->cache_size = cache_size;


   new_hash_size = 63;
   if(hash_size > 63) {
       while(new_hash_size<hash_size && new_hash_size < 0xFFFFFF){
	   new_hash_size++; 
	   new_hash_size = (new_hash_size << 1) -1;
       }
   }
   printf ("Hash size: %d\n",new_hash_size);
   cache->hash_table=calloc(new_hash_size,sizeof(struct ci_cache_entry *));
   cache->hash_table_size = new_hash_size; 
   cache->ttl = ttl;
   cache->compare = compare;
   cache->release = release;
   return cache;
}

void *ci_cache_search(struct ci_cache_table *cache,void *key,int key_len) {
    struct ci_cache_entry *e;
    unsigned int hash=compute_hash(cache, key, key_len);

    if(hash >= cache->hash_table_size) /*is it possible?*/
	return NULL;

    e=cache->hash_table[hash];
    while(e!=NULL) {
	printf(" \t\t->>>>Val %s\n",e->val);
	if(cache->compare(e->key, e->keylen, key, key_len))
           return e->val;
       e=e->hnext;
    }
   return NULL;
}

void *ci_cache_update(struct ci_cache_table *cache, void *key, int key_len, void *val) {
    struct ci_cache_entry *e,*tmp;
    unsigned int hash=compute_hash(cache, key, key_len);

    printf("Adding :%s:%s\n",key,val);

    if (cache->cache_size) { /*It is a cache with static size */
        /*Get the oldest entry (TODO:check the cache ttl value if exists)*/
    	e=cache->first_queue_entry;
    	cache->first_queue_entry=cache->first_queue_entry->qnext;
        /*Make it the newest entry (make it last entry in queue)*/
        cache->last_queue_entry->qnext = e;
        cache->last_queue_entry = e;
        e->qnext = NULL;
            
        /*If it has data release its data*/
    	if (e->key)
	     cache->release(e->key, e->val);
        /*If it is in the hash table remove it...*/
    	if(e->hash) {
		tmp = cache->hash_table[e->hash];
		if(tmp == e)
	    		cache->hash_table[e->hash] = tmp->hnext;
		else if(tmp) {
	    		while(tmp->hnext != NULL && e != tmp->hnext) tmp = tmp->hnext;
	    		if(tmp->hnext)
				tmp->hnext = tmp->hnext->hnext;
		}
	}
        
    }
    else { /*is a cache with variable size so just allocate new entry and put it to hash*/
        e = malloc(sizeof(struct ci_cache_entry));
        e->qnext = NULL; /*not used here*/
    }
    
    
    e->hnext = NULL;
    e->time = ci_internal_time();
    e->key=key;
    e->keylen=key_len;
    e->val=val;
    e->hash=hash;
   

    if(cache->hash_table[hash])
	printf("\t\t:::Found %s\n",cache->hash_table[hash]->val);
    /*Make it the first entry in the current hash entry*/
    e->hnext=cache->hash_table[hash];
    cache->hash_table[hash] = e;
}


#ifdef NOT_USED

int strmatch(void *key1, int keylen1, void *key2, int keylen2)
{
    char *skey1=(char *)key1, *skey2=(char *)key2;
    return (strcmp(skey1,skey2)==0);
}

void strrelease(void *key, void *val){
    printf("FReeing mem for :%s\n",(char *)val);
    free(val);
}



int main(int argc,char *argv[]) {
    struct ci_cache_table *cache;
    char *s;
    printf("Hi re\n");
    cache = ci_cache_build(3,3,10,0,strmatch,strrelease);

    s=strdup("test1");
    ci_cache_update(cache,s,0,s);

    s=strdup("test2");
    ci_cache_update(cache,s,0,s);

    s=strdup("test3");
    ci_cache_update(cache,s,0,s);


    s=strdup("test4");
    cache_update(cache,s,0,s);


    s=ci_cache_search(cache,"test2",0);
    printf("Found : %s\n",s);

    s=ci_cache_search(cache,"test21",0);
    printf("Found : %s\n",s);

    s=ci_cache_search(cache,"test1",0);
    printf("Found : %s\n",s);

    s=ci_cache_search(cache,"test4",0);
    printf("Found : %s\n",s);

}


#endif
