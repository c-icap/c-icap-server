#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "c-icap.h"
#include "debug.h"
#include "filetype.h"



struct ci_data_type predefined_types[]={
     {"ASCII","ASCII text file",CI_TEXT_DATA},
     {"ISO-8859","ISO-8859 text file",CI_TEXT_DATA},
     {"EXT-ASCII","Extended ASCII (Mac,IBM PC etc.)",CI_TEXT_DATA},
     {"UTF","Unicode text file ",CI_TEXT_DATA},
     {"HTML","HTML text",CI_TEXT_DATA},
     {"BINARY","Unknown data",CI_OCTET_DATA},
     {"","",0}
};

struct ci_data_group predefined_groups[]={
     {"TEXT","All texts"},
     {"DATA","Undefined data type"},
     {"",""}
};




struct ci_magic_record{
     int offset;
     unsigned char magic[MAGIC_SIZE+1];
     size_t len;
     char type[NAME_SIZE+1];
     char group[NAME_SIZE+1];
     char descr[DESCR_SIZE+1];
};





#define DECLARE_ARRAY_FUNCTIONS(structure,array,type,size) int array##_init(structure *db){ \
                                                     if((db->array=malloc(size*sizeof(type)))==NULL) \
	                                                  return 0; \
                                                     db->array##_num=0; \
                                                     db->array##_size=size;\
                                                     return 1; \
                                                    }


#define CHECK_SIZE(db,array,type,size)   if(db->array##_num >= db->array##_size){\
	                                           if((newdata=realloc(db->array,db->array##_size+size*sizeof(type)))==NULL)\
	                                                     return -1;\
	                                            db->array =newdata;\
                                        }


DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db,types,struct ci_data_type,50)
DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db,groups,struct ci_data_group,15)
DECLARE_ARRAY_FUNCTIONS(struct ci_magics_db,magics,struct ci_magic,50)


int types_add(struct ci_magics_db *db, char *name,char *descr,int group){
     struct ci_data_type *newdata;
     int indx;

     CHECK_SIZE(db,types,struct ci_data_type,50);
     
     indx=db->types_num;
     db->types_num++;
     strcpy(db->types[indx].name,name);
     strcpy(db->types[indx].descr,descr);
     db->types[indx].group=group;
     return indx;
}


int groups_add(struct ci_magics_db *db, char *name,char *descr){
     struct ci_data_group *newdata;
     int indx;

     CHECK_SIZE(db,groups,struct ci_data_group,15)

     indx=db->groups_num;
     db->groups_num++;
     strcpy(db->groups[indx].name,name);
     strcpy(db->groups[indx].descr,descr);
     return indx;
}

int magics_add(struct ci_magics_db *db,int offset,char *magic,int len,int type){
     struct ci_magic *newdata;
     int indx;

     CHECK_SIZE(db,magics,struct ci_magic,50)

     indx=db->magics_num;
     db->magics_num++;

     db->magics[indx].type=type;
     db->magics[indx].offset=offset;
     db->magics[indx].len=len;
     memcpy(db->magics[indx].magic,magic,len);
     
     return indx;
}





int ci_get_data_type_id(struct ci_magics_db *db,char *name){
     int i=0;
     for(i=0;i<db->types_num;i++){
	  if(strcasecmp(name,db->types[i].name)==0)
	       return i;
     }
     return -1;
}

int ci_get_data_group_id(struct ci_magics_db *db,char *group){
     int i=0;
     for(i=0;i<db->groups_num;i++){
	  if(strcasecmp(group,db->groups[i].name)==0)
	       return i;
     }
     return -1;
}

int read_record(FILE *f,struct ci_magic_record *record){
     char line[512],*s,*end,num[4];
     int len,c,i;
     
     if(fgets(line,512,f)==NULL)
	  return -1;
     if((len=strlen(line))<4) /*must have at least 4 ':' */
	  return 0;
     if(line[0]=='#') /*Comment .......*/
	  return 0;
     line[--len]='\0'; /*the \n at the end of*/
     s=line;
     errno=0;
     record->offset=strtol(s,&end,10);
     if(*end!=':' || errno!=0)
	  return 0;

     s=end+1;
     i=0;
     end=line+len;
     while(*s!=':' && s<end && i< MAGIC_SIZE){
	  if(*s=='\\'){
	       s++;
	       if(*s=='x'){
		    s++;
		    num[0]=*(s++);
		    num[1]=*(s++);
		    num[2]='\0';
		    c=strtol(num,NULL,16);
	       }
	       else{
		    num[0]=*(s++);
		    num[1]=*(s++);
		    num[2]=*(s++);
		    num[3]='\0';
		    c=strtol(num,NULL,8);
	       }
	       if(c>256 || c<0){
		    return -2;
	       }
	       record->magic[i++]=c;
	  }
	  else{
	       record->magic[i++]=*s;
	       s++;
	  }
     }
     record->len=i;
     
     if(s>=end|| *s!=':'){ /*End of the line..... parse error*/
	  return -2;
     }
     s++;
     if((end=strchr(s,':'))==NULL){
	  return -2; /*Parse error*/
     }
     *end='\0';
     strncpy(record->type,s,NAME_SIZE);
     record->type[NAME_SIZE]='\0';
     s=end+1;

     if((end=strchr(s,':'))==NULL){
	  return -2; /*Parse error*/
     }
     *end='\0';
     strncpy(record->group,s,NAME_SIZE);
     record->group[NAME_SIZE]='\0';

     s=end+1;
     strncpy(record->descr,s,DESCR_SIZE);
     record->descr[DESCR_SIZE]='\0';

     return 1;
}

struct ci_magics_db *ci_magics_db_init(){
     struct ci_magics_db *db;
     int i;
     db=malloc(sizeof(struct ci_magics_db));
     types_init(db);
     groups_init(db);
     magics_init(db);

     i=0;/*Copy predefined types*/
     while(predefined_types[i].name[0]!='\0'){
	  types_add(db,predefined_types[i].name,predefined_types[i].descr,predefined_types[i].group);
	  i++;
     }

     i=0;/*Copy predefined groups*/
     while(predefined_groups[i].name[0]!='\0'){
	  groups_add(db,predefined_groups[i].name,predefined_groups[i].descr);
	  i++;
     }


     return db;
}

void ci_magics_db_release(struct ci_magics_db *db){
     free(db->types);
     free(db->groups);
     free(db->magics);
     free(db);
}


int ci_magics_db_file_add(struct ci_magics_db *db,char *filename){
     int type,group,ret;
     struct ci_magic_record record;
     FILE *f;

     if((f=fopen(filename,"r+"))==NULL){
	  ci_debug_printf(1,"Error opening magic file: %s\n",filename);
	  return 0;
     }
     while((ret=read_record(f,&record))>=0){
	  if(!ret)
	       continue;
	  if((type=ci_get_data_type_id(db,record.type))<0){
	       if((group=ci_get_data_group_id(db,record.group))<0){
		    group=groups_add(db,record.group,"");
	       }
	       type=types_add(db,record.type,record.descr,group);
	  }
	  
	  magics_add(db,record.offset,record.magic,record.len,type);
     }
     fclose(f);
     if(ret<-1){/*An error occured .....*/
	  ci_debug_printf(1,"Error reading magic file (%d)\n",ret);
	  return 0;
     }
     ci_debug_printf(3,"In database magics:%d, types:%d, groups:%d\n",db->magics_num,db->types_num,db->groups_num);
     return 1;


}


struct ci_magics_db *ci_magics_db_build(char *filename){
     struct ci_magics_db *db;

     
     if((db=ci_magics_db_init())!=NULL)
	  ci_magics_db_file_add(db,filename);
     return db;
}


int check_magics(struct ci_magics_db *db,char *buf, int buflen){
     int i;
     for(i = 0; i<db->magics_num; i++) {
	  if(buflen >= db->magics[i].offset+db->magics[i].len) {
	       if(memcmp(buf+db->magics[i].offset, db->magics[i].magic, db->magics[i].len) == 0) {
		    return db->magics[i].type;
	       }
	  }
     }
     return -1;
}

/*The folowing table taking from the file project........*/

/*0 are the characters which never appears in text */
#define T 1   /* character appears in plain ASCII text */
#define I 2   /* character appears in ISO-8859 text */
#define X 4   /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static const char text_chars[256] = {
     /*                  BEL BS HT LF    FF CR    */
     0, 0, 0, 0, 0, 0, 0, T, T, T, T, 0, T, T, 0, 0,  /* 0x0X */
     /*                              ESC          */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, T, 0, 0, 0, 0,  /* 0x1X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
     T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, 0,  /* 0x7X */
     /*            NEL                            */
     X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,  /* 0x8X */
     X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x9X */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xaX */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xbX */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xcX */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xdX */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xeX */
     I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I   /* 0xfX */
};


/*
ASCII if res<=1
ISO if res<=3
EXTEND if res<=7
*/

int check_ascii(unsigned char *buf,int buflen){
     unsigned int i,res=0,type;
     for(i=0;i<buflen;i++){ /*May be only a small number (30-50 bytes) of the first data must be checked*/
	  if((type=text_chars[buf[i]])==0)
	       return -1;
	  res=res|type;
     }
     if(res<=1)
	  return CI_ASCII_DATA;
     if(res<=3)
	  return CI_ISO8859_DATA;
     
     return CI_XASCII_DATA;
}





int check_unicode(unsigned char *buf,int buflen){
     return -1;
}




int ci_filetype(struct ci_magics_db *db,char *buf, int buflen){
     int i,ret, ascii = 1, len;

     if((ret=check_magics(db,buf,buflen))>=0)
	  return ret;
     
     if((ret=check_ascii((unsigned char *)buf,buflen))<0)
	  return CI_BIN_DATA; /*binary data*/
     return ret;
}
