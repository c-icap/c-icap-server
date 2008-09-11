#include "lookup_table.h"
#include "debug.h"
//#include <string.h>

/******************************************************/
/* file lookup table implementation                   */

void *file_table_open(struct ci_lookup_table *table); 
void  file_table_close(struct ci_lookup_table *table);
void **file_table_search(struct ci_lookup_table *table, void *key);
void  file_table_release_result(struct ci_lookup_table *table_data,void **val);

struct ci_lookup_table_type file_table_type={
    file_table_open,
    file_table_close,
    file_table_search,
    file_table_release_result,
    "file"
};


struct text_table_entry {
    char *key;
    char **vals;
    struct text_table_entry *next;
};

struct text_table {
     struct text_table_entry *entries;
     int rows;
};

struct text_table_entry *read_row(FILE *f, int cols, 
				  ci_mem_allocator_t *allocator,
				  void *(*keydup)(const char *, ci_mem_allocator_t *),
				  void *(*valdup)(const char *, ci_mem_allocator_t *) ) 
{
     struct text_table_entry *e;
     char line[65536];
     char *s,*val,*end;
     int row_cols,line_len,i;
     if(!fgets(line,65535,f))
          return NULL;
     line[65535]='\0';

     if((line_len=strlen(line))>65535) {
        line[64]='\0';
        ci_debug_printf(1,"Too long line: %s...",line); 
        return NULL;
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
    
     e = malloc(sizeof(struct text_table_entry)); 
     if (row_cols>1) 
         e->vals = malloc((row_cols)*sizeof(char *));
     else
         e->vals = NULL; /*Only key */

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
     e->key=keydup(val, allocator);

     if(row_cols!=1) {
         for (i=0; *s!='\0'; i++) { /*probably we have vals*/
             if(i>=row_cols) {
                ci_debug_printf(1,"What the hell hapens!!!!!\n");
                return NULL;
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
              e->vals[i] = valdup(val, allocator);
          }
          e->vals[i]='\0';
     }
     return e;
}

int load_text_table(char *filename, struct ci_lookup_table *table) {
     FILE *f;
     struct text_table_entry *e;
     int rows;
     struct text_table *text_table = (struct text_table *)table->data;
     if ((f = fopen(filename, "r+")) == NULL) {
	 ci_debug_printf(1, "Error opening file: %s\n", filename);
	 return 0;
     }
     rows = 0;
     while((e=read_row(f,table->cols,table->allocator, table->keydup,table->valdup))) {
         e->next = text_table->entries;
         text_table->entries = e;
         rows++;
     }

     fclose(f);

     text_table->rows = rows;
     return 1;
}


void *file_table_open(struct ci_lookup_table *table)
{
  struct text_table *text_table=malloc(sizeof(struct text_table));

  if(!text_table)
    return NULL;

  text_table->entries=NULL;
  table->data=(void *)text_table;
  if(!load_text_table(table->path, table)) {
    free(text_table);
    return (table->data=NULL);
  }
  return text_table;
}

void  file_table_close(struct ci_lookup_table *table)
{
    int i;
    void **vals;
    struct text_table_entry *e,*tmp;
    struct text_table *text_table=(struct text_table *)table->data;
    e=text_table->entries;
    
    while(e) {
	tmp=e;
	e = e->next;
	vals=(void **)tmp->vals;
	for(i=0;vals[i]!=NULL;i++)
	    free(vals[i]);
	
	free(tmp->vals);
	free(tmp->key);
	free(tmp);
    }
    free(text_table);
    table->data=NULL;
}

void **file_table_search(struct ci_lookup_table *table, void *key)
{
  struct text_table_entry *e;
  struct text_table *text_table=(struct text_table *)table->data;
  e=text_table->entries;
  while(e) {
      if(table->keycomp((void *)e->key,key)==0)
      return (void **)e->vals;
    e = e->next;
  }
  return NULL;
}

void  file_table_release_result(struct ci_lookup_table *table_data,void **val)
{
  /*do nothing*/
}


#if 0
int main(int argc, char **argv) {
  struct ci_lookup_table *table;
  struct text_table *text_table;
  struct text_table_entry *e;
  void **vals;
  int i;
  
  ci_lookup_table_type_add(&file_table_type);
  
  if(argc<2) exit(-1);
  
  table=ci_lookup_table_create(argv[1]);

  if(!table) {
    printf("Table :%s not found!!!\n",argv[1]);
    exit(-1);
  }

  if(!table->open(table)) {
    printf("Can not open table %s\n",argv[1]);
    exit(-2);
  }

  printf("A simple search:\n");
  vals=table->search(table, "test3", 0);
  if(!e)
    printf("E with key %s not found!\n","test3");
  else {
    printf("Key: %s ", "test3");
    if(e->vals) {
      printf("|Values: ");
      i=0;
      while(vals[i]) printf("%s ", vals[i++]);
    }
    printf("\n\n");
  }

  text_table=(struct text_table *)table->data;

   e=text_table->entries;
   while(e) {
       printf("Key: %s ", e->key);
       if(e->vals) {
          printf("|Values: ");
          i=0;
          while(e->vals[i]) printf("%s ", e->vals[i++]);
       }
       printf("\n");
       e = e->next;
   }
}
#endif
