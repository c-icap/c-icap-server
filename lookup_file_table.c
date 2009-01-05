#include "lookup_table.h"
#include "hash.h"
#include "debug.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

/******************************************************/
/* file lookup table implementation                   */

void *file_table_open(struct ci_lookup_table *table); 
void  file_table_close(struct ci_lookup_table *table);
void *file_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  file_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type file_table_type={
    file_table_open,
    file_table_close,
    file_table_search,
    file_table_release_result,
    "file"
};

struct text_table_entry {
    void *key;
    void **vals;
    struct text_table_entry *next;
};

struct text_table {
    struct text_table_entry *entries;
    struct ci_hash_table *hash_table;
    int rows;
};

int read_row(FILE *f, int cols, struct text_table_entry **e,
				  ci_mem_allocator_t *allocator,
				  ci_type_ops_t *key_ops,
				  ci_type_ops_t *val_ops)
{
     char line[65536];
     char *s,*val,*end;
     int row_cols,line_len,i;

     (*e)=NULL;

     if(!fgets(line,65535,f))
          return 0;
     line[65535]='\0';
     
     if((line_len=strlen(line))>65535) {
        line[64]='\0';
        ci_debug_printf(1, "Too long line: %s...", line); 
        return 0;
     }
     if(line[line_len-1]=='\n') line[line_len-1]='\0'; /*eat the newline char*/ 

     if(cols<0) {
         /*the line should have the format  key:val1, val2, ... */
         if (!(s=index(line,':'))) {
             row_cols=1;
         }
         else {
            row_cols=2;
            while((s=index(s,','))) row_cols++,s++;
         }
     }
     else
        row_cols=cols;
    
     (*e) = allocator->alloc(allocator, sizeof(struct text_table_entry)); 
     if(!(*e)) {
	 ci_debug_printf(1,"Error allocating memory for table entry:%s\n", line);
	 return 0;
     }
     if (row_cols>1) {
         (*e)->vals = allocator->alloc(allocator, (row_cols)*sizeof(char *));
	 if(!(*e)->vals) {
	     allocator->free(allocator,(*e));
	     (*e)=NULL;
	     ci_debug_printf(1,"Error allocating memory for values of  table entry:%s\n", line);
	     return 0;
	 }
     }
     else
         (*e)->vals = NULL; /*Only key */

     s=line;
    
     while(*s==' ' || *s == '\t') s++;
     val=s; 
     
     if( row_cols==1 ) 
        end=s+strlen(s);
     else
        end=index(s,':');

     s=end+1; /*Now points to the end (*s='\0') or after the ':' */
      
     end--; 
     while(*end==' ' || *end=='\t') end--;
     *(end+1)='\0';
     (*e)->key=key_ops->dup(val, allocator);

     if(row_cols!=1) {
         for (i=0; *s!='\0'; i++) { /*probably we have vals*/
             if(i>=row_cols) {
		 /*here we are leaving memory leak, I think qill never enter this if ...*/
		 ci_debug_printf(1, "Error in read_row of file lookup table!(line:%s!!!)\n", line);
		 return 0;
              }

              while(*s==' ' || *s =='\t') s++; /*find the start of the string*/
              val = s; 
              end=s;
              while(*end!=',' && *end!='\0') end++; 
              if (*end=='\0') 
                  s = end;
              else
                  s = end + 1;

              end--;
              while(*end == ' ' || *end == '\t') end--;
              *(end+1)='\0';
              (*e)->vals[i] = val_ops->dup(val, allocator);
          }
	 (*e)->vals[i]='\0';
     }
     return 1;
}

int load_text_table(char *filename, struct ci_lookup_table *table) {
     FILE *f;
     struct text_table_entry *e, *l = NULL;
     int rows, ret;
     struct text_table *text_table = (struct text_table *)table->data;
     if ((f = fopen(filename, "r+")) == NULL) {
	 ci_debug_printf(1, "Error opening file: %s\n", filename);
	 return 0;
     }
     rows = 0;
     while(0 != (ret=read_row(f, table->cols, &e,
			       table->allocator, 
			       table->key_ops,
			       table->val_ops))) {
	 e->next = NULL;
	 if(text_table->entries==NULL) {
	     text_table->entries = e;
	     l = e;
	 }
	 else {
	     l->next = e;
	     l = e;
	 }
         rows++;
     }
     fclose(f);

     if(ret==-1) {
	 file_table_close(table);
	 return 0;
     }

     text_table->rows = rows;
     return 1;
}


void *file_table_open(struct ci_lookup_table *table)
{
  struct ci_mem_allocator *allocator = table->allocator;
  struct text_table *text_table=allocator->alloc(allocator, sizeof(struct text_table));

  if(!text_table)
    return NULL;

  text_table->entries=NULL;
  table->data=(void *)text_table;
  if(!load_text_table(table->path, table)) {
    return (table->data=NULL);
  }
  text_table->hash_table = NULL;
  return text_table;
}

void  file_table_close(struct ci_lookup_table *table)
{
    int i;
    void **vals = NULL;
    struct text_table_entry *e,*tmp;
    struct ci_mem_allocator *allocator = table->allocator;
    struct text_table *text_table = (struct text_table *)table->data;

    if(!text_table) {
	ci_debug_printf(1,"file lookup table is not open?\n");
	return;
    }
    e=text_table->entries;
    
    while(e) {
	tmp = e;
	e = e->next;
	if(tmp->vals) {
	    vals=(void **)tmp->vals;
	    for(i=0;vals[i]!=NULL;i++)
		table->val_ops->free(vals[i], allocator);
	    allocator->free(allocator, tmp->vals);
	}
      
	table->key_ops->free(tmp->key, allocator);
	allocator->free(allocator, tmp);
    }
    allocator->free(allocator, text_table);
    table->data=NULL;
}

void *file_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
  struct text_table_entry *e;
  struct text_table *text_table=(struct text_table *)table->data;

  if(!text_table) {
      ci_debug_printf(1,"file lookup table is not open?\n");
      return NULL;
  }

  e=text_table->entries;
  *vals=NULL;
  while(e) {
      if (table->key_ops->compare((void *)e->key,key)==0) {
           *vals=(void **)e->vals;
           return (void *)e->key;
      }
    e = e->next;
  }
  return NULL;
}

void  file_table_release_result(struct ci_lookup_table *table_data,void **val)
{
  /*do nothing*/
}

/******************************************************/
/* hash lookup table implementation                   */

void *hash_table_open(struct ci_lookup_table *table); 
void  hash_table_close(struct ci_lookup_table *table);
void *hash_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  hash_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type hash_table_type={
    hash_table_open,
    hash_table_close,
    hash_table_search,
    hash_table_release_result,
    "hash"
};


void *hash_table_open(struct ci_lookup_table *table)
{
    struct text_table_entry *e;
    struct text_table *text_table=file_table_open(table);
    if (!text_table)
	return NULL;

    /* build the hash table*/
    text_table->hash_table = ci_hash_build(text_table->rows, 
					   table->key_ops, 
					   table->allocator);
    if(!text_table->hash_table) {
	file_table_close(table);
	return NULL;
    }
    
    e=text_table->entries;
    while(e) {
	ci_hash_add(text_table->hash_table,e->key, e);
	e = e->next;
    }

    return text_table;
}

void  hash_table_close(struct ci_lookup_table *table)
{
    struct text_table *text_table = (struct text_table *)table->data;
    /*destroy the hash table */
    ci_hash_destroy(text_table->hash_table);
    /*... and then call the file_table_close:*/
    file_table_close(table);
}

void *hash_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
    struct text_table_entry *e;
    struct text_table *text_table = (struct text_table *)table->data;

    if(!text_table) {
	ci_debug_printf(1,"file lookup table is not open?\n");
	return NULL;
    }

    *vals=NULL;
    e = ci_hash_search(text_table->hash_table, key);
    if(!e)
	return NULL;

    *vals = (void **)e->vals;
    return (void *)e->key;
}

void  hash_table_release_result(struct ci_lookup_table *table_data,void **val)
{
    /*do nothing*/
}

/******************************************************/
/* regex lookup table implementation                   */

void *regex_table_open(struct ci_lookup_table *table); 
void  regex_table_close(struct ci_lookup_table *table);
void *regex_table_search(struct ci_lookup_table *table, void *key, void ***vals);
void  regex_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type regex_table_type={
    regex_table_open,
    regex_table_close,
    regex_table_search,
    regex_table_release_result,
    "regex"
};


/*regular expresion operator definition  */
#ifdef HAVE_REGEX
/*We only need the preg field which holds the compiled regular expression 
  but keep the uncompiled string too just for debuging reasons */
struct ci_regex {
    char *str;
    int flags;
    regex_t preg;
};

/*Parse the a regular expression in the form: /regexpression/flags
  where flags nothing or 'i'. Examples:
        /^\{[a-z| ]*\}/i
	/^some test.*t/
*/
void *regex_dup(const char *str, ci_mem_allocator_t *allocator)
{
    struct ci_regex *reg;
    char *newstr,*s;
    int slen;
    unsigned flags;

    s=(char *)str;

    if(!*s=='/') {
	ci_debug_printf(1, "Parse error, regex should has the form '/expresion/flags'");
	return NULL;
    }
    s++;
    slen=strlen(s);
    newstr = allocator->alloc(allocator,slen+1);    
    if(!newstr) {
	ci_debug_printf(1,"Error allocating memory for regex_dup!\n");
	return NULL;
    }
    strcpy(newstr, s);
    s=newstr+slen; /*Points to the end of string*/
    while(*s!='/' && s!=newstr) s--;

    if(s==newstr) {
	ci_debug_printf(1,"Parse error, regex should has the form '/expression/flags' (regex=%s)!\n",newstr);
	allocator->free(allocator, newstr);
	return NULL;
    }
    /*Else found the last '/' char:*/
    *s='\0';
    /*parse flags:*/
    flags=0;
    s++;
    while(*s!='\0') {
	if(*s=='i')
	    flags = flags | REG_ICASE;
	else { /*other flags*/
	}
    }
    flags |= REG_EXTENDED; /*or beter the 'e' option?*/
    flags |= REG_NOSUB; /*we do not need it*/
    
    reg = allocator->alloc(allocator,sizeof(struct ci_regex));
    if(!reg) {
	ci_debug_printf(1,"Error allocating memory for regex_dup (1)!\n");
	allocator->free(allocator, newstr);
	return NULL;
    }

    if (regcomp(&(reg->preg), newstr, flags) != 0) {
	ci_debug_printf(1, "Error compiling regular expression :%s (%s)\n", str, newstr);
	allocator->free(allocator, reg);
	allocator->free(allocator, newstr);
	return NULL;
    }

    reg->str = newstr;
    reg->flags = flags;

    return reg;
}

/*
The following method is a litle bit dangerous because the key1 and key2 had different types
  but OK it is a temporary ci_type_ops object
*/

int regex_cmp(void *key1,void *key2)
{
    regmatch_t pmatch[1];
    struct ci_regex *reg=(struct ci_regex *)key1;
    return regexec(&reg->preg, (char *)key2, 1, pmatch, 0);
}

size_t regex_len(void *key)
{
    return strlen(((const struct ci_regex *)key)->str);
}

void regex_free(void *key, ci_mem_allocator_t *allocator)
{
    struct ci_regex *reg=(struct ci_regex *)key;
    regfree(&(reg->preg));
    allocator->free(allocator, reg->str);
    allocator->free(allocator, reg);
}


ci_type_ops_t  ci_regex_ops = {
    regex_dup,
    regex_cmp,
    regex_len,
    regex_free
};
#endif




void *regex_table_open(struct ci_lookup_table *table)
{
#ifdef HAVE_REGEX
    struct text_table *text_table;
    if(table->key_ops != &ci_str_ops) {
	ci_debug_printf(1,"This type of table is not compatible with regex tables!\n");
	return NULL;
    }
    table->key_ops = &ci_regex_ops;
    
    text_table=file_table_open(table);
    if (!text_table)
	return NULL;
    
    return text_table;
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
    return NULL;
#endif
}

void  regex_table_close(struct ci_lookup_table *table)
{
#ifdef HAVE_REGEX
    /*just call the file_table_close:*/
    file_table_close(table);
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
    return NULL;
#endif
}

void *regex_table_search(struct ci_lookup_table *table, void *key, void ***vals)
{
#ifdef HAVE_REGEX
    return file_table_search(table, key, vals);
#else
    ci_debug_printf(1,"regex lookup tables are not supported on this system!\n");
    return NULL;
#endif
}

void  regex_table_release_result(struct ci_lookup_table *table_data,void **val)
{
    /*do nothing*/
}

