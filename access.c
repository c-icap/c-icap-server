/*
 *  Copyright (C) 2004 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "c-icap.h"
#include "net_io.h"
#include "request.h"
#include "module.h"
#include "cfg_param.h"
#include "debug.h"
#include "access.h"
#include "simple_api.h"


access_control_module_t **used_access_controllers=NULL;


/* #define MAX_USERNAME_LEN 256 */


int access_check_client(ci_connection_t *connection){
     int i=0,res;
     struct sockaddr_in *client_address, *server_address;
     if(!used_access_controllers)
	  return CI_ACCESS_ALLOW;
     client_address=&(connection->claddr);
     server_address=&(connection->srvaddr);
     i=0;
     while(used_access_controllers[i]!=NULL){
	  if(used_access_controllers[i]->client_access){
	       res=used_access_controllers[i]->client_access(client_address,server_address);
	       if(res!=CI_ACCESS_UNKNOWN) 
		    return res;
	  }
	  i++;
     }
     return CI_ACCESS_ALLOW;
}



int access_check_request(request_t *req){
     char *user;
     int i,res;

     if(!used_access_controllers)
	  return CI_ACCESS_ALLOW;

     user=get_header_value(req->head,"X-Authenticated-User");
     if(user){
	  ci_base64_decode(user,req->user,MAX_USERNAME_LEN);
	  debug_printf(10,"The user is:%s\n",req->user);
     }

     i=0;
     while(used_access_controllers[i]!=NULL){
	  if(used_access_controllers[i]->request_access){
	       res=used_access_controllers[i]->request_access(req->user,req->service,
							 &(req->connection->claddr),&(req->connection->srvaddr));
	       if(res!=CI_ACCESS_UNKNOWN) 
		    return res;
	  }
	  i++;
     }

     return CI_ACCESS_ALLOW;
}


int access_authenticate_request(request_t *req){
     return CI_ACCESS_ALLOW;
}



/*********************************************************************************************/
/* Default Authenticator                                                                      */
int  default_acl_init();
void default_release_authenticator();
int  default_acl_client_access(struct sockaddr_in *client_address, struct sockaddr_in *server_address);
int  default_acl_request_access(char *dec_user,char *service,
				struct sockaddr_in *client_address, 
				struct sockaddr_in *server_address);
int default_acl_auth_access(char *dec_user,char *service,
			    struct sockaddr_in *client_address, 
			    struct sockaddr_in *server_address);



int acl_add(char *directive,char **argv,void *setdata);
int acl_access(char *directive,char **argv,void *setdata);



/*Configuration Table .....*/
static struct conf_entry acl_conf_variables[]={
     {"acl",NULL,acl_add,NULL},
     {"icap_access",NULL,acl_access,NULL},
     {NULL,NULL,NULL,NULL}
};


access_control_module_t default_acl={
     "default_acl",
     &default_acl_init,
     &default_release_authenticator,
     &default_acl_client_access,
     &default_acl_request_access,
     &default_acl_auth_access,
     acl_conf_variables
};

#define MAX_NAME_LEN 31

typedef struct acl_spec acl_spec_t;
struct acl_spec{
     char name[MAX_NAME_LEN+1];
     char username[MAX_USERNAME_LEN+1];
     char servicename[256]; /*There is no limit for the length of service name, but I do not believe that
                               it can exceed 256 bytes .......*/
     unsigned int port;
     unsigned long hclient_address; /*unsigned 32 bit integer */
     unsigned long hclient_netmask;
     unsigned long hserver_address;
     acl_spec_t *next;
};


typedef struct access_entry access_entry_t;
struct access_entry{
     int type;/*CI_ACCESS_DENY or CI_ACCESS_ALLOW or CI_ACCES_AUTH*/

     acl_spec_t *spec;
     access_entry_t *next;
};


acl_spec_t *acl_spec_list=NULL;
acl_spec_t *acl_spec_last=NULL;
access_entry_t *access_entry_list=NULL;
access_entry_t *access_entry_last=NULL;
int match_connection(acl_spec_t *spec,in_port_t srvport,struct in_addr *client_address, 
		     struct in_addr *server_address);
int match_request(acl_spec_t *spec, char *dec_user, char *service, in_port_t srvport,
		  struct in_addr *client_address, struct in_addr *server_address);



int default_acl_init(){
     acl_spec_list=NULL; /*Not needed ......*/
     acl_spec_last=NULL;
     access_entry_list=NULL;
     access_entry_last=NULL;
}

void default_release_authenticator(){
     /*Must release the queues ........*/
}

int default_acl_client_access(struct sockaddr_in *client_address, struct sockaddr_in *server_address){
     access_entry_t *entry;
     acl_spec_t *spec;

     entry=access_entry_list;
     while(entry){
	  spec=entry->spec;
	  if(match_connection(spec,
			      server_address->sin_port,
			      &(client_address->sin_addr),
			      &(server_address->sin_addr))){
	       if(entry->type==CI_ACCESS_HTTP_AUTH){
		    return CI_ACCESS_HTTP_AUTH;
	       }
	       else if(spec->username[0]=='\0' && spec->servicename[0]=='\0'){/* So no user or service 
										 check needed*/
		    return entry->type;
	       }
	       else
		    return CI_ACCESS_PARTIAL; /*If service or username needed for this spec
                                                must get icap headers for this connection*/
	  }
	  entry=entry->next;
     }
     return CI_ACCESS_UNKNOWN;

}


int default_acl_request_access(char *dec_user,char *service,
			     struct sockaddr_in *client_address, 
			     struct sockaddr_in *server_address){
     access_entry_t *entry;
     acl_spec_t *spec;
     
     entry=access_entry_list;
     while(entry){
	  spec=entry->spec;
	  if(match_request(spec, dec_user, service, server_address->sin_port,
			   &(client_address->sin_addr),
			   &(server_address->sin_addr))){
	       return entry->type;
	  }
	  entry=entry->next;
     }
     return CI_ACCESS_DENY;
}


int default_acl_auth_access(char *dec_user,char *service,
			     struct sockaddr_in *client_address, 
			     struct sockaddr_in *server_address){
     /*Must implemented ........*/
     return CI_ACCESS_DENY;
}



/*********************************************************************/
/*ACL list managment functions                                       */


int match_connection(acl_spec_t *spec,in_port_t srvport,struct in_addr *client_address, 
		                                         struct in_addr *server_address){
     unsigned long hmask;

     hmask=spec->hclient_netmask;

     if(spec->port!=0 && spec->port != srvport)
	  return 0;

     if(spec->hserver_address!=0 && spec->hserver_address != server_address->s_addr)
	  return 0;
     
     if( (spec->hclient_address & hmask)!=0 && 
	 (spec->hclient_address & hmask)!=(client_address->s_addr & hmask))
	  return 0;

     return 1;
}

int match_request(acl_spec_t *spec, char *dec_user, char *service, in_port_t srvport,
		  struct in_addr *client_address, 
		  struct in_addr *server_address){

     if(!match_connection(spec,srvport,client_address,server_address))
	  return 0;

     if(spec->servicename && strcmp(spec->servicename,service)!=0)
	  return 0;
     
     if(spec->username){
	  if(dec_user[0]=='\0')
	       return 0;
	  if(strcmp(spec->username,"*")==0) /*All users ......*/
	       return 1;
	  if(strcmp(spec->username,dec_user)!=0)
	       return 0;
     }
     
     return 1;
}


acl_spec_t *find_acl_spec_byname(char *name){
     acl_spec_t *spec;
     if(acl_spec_list==NULL)
	  return NULL;
     for(spec=acl_spec_list;spec!=NULL;spec=spec->next){
	  if(strcmp(spec->name,name)==0)
	       return spec;
     }
     return NULL;
}


access_entry_t *new_access_entry(int type,char *name){
     access_entry_t *a_entry;
     acl_spec_t *spec;

     if((spec=find_acl_spec_byname(name))==NULL)
	  return NULL;

     /*create the access entry .......*/
     if((a_entry=malloc(sizeof(access_entry_t)))==NULL)
	  return NULL;
     a_entry->next=NULL;
     a_entry->type=type;
     a_entry->spec=spec;

     /*Add access entry to the end of list ........*/
     if(access_entry_list==NULL){
	  access_entry_list=a_entry;
	  access_entry_last=a_entry;
     }
     else{
	  access_entry_last->next=a_entry;
	  access_entry_last=a_entry;
     }

     debug_printf(10,"ACL entry %s %d  added\n",name,type);
     return a_entry;
}

acl_spec_t *new_acl_spec(char *name,char *username, int port,char *service,   
			 struct in_addr *client_address,
			 struct in_addr *client_netmask,
			 struct in_addr *server_address){
     acl_spec_t *a_spec;
     unsigned long haddr,hmask;
     if((a_spec=malloc(sizeof(acl_spec_t)))==NULL)
	  return NULL;
     a_spec->next=NULL;
     strncpy(a_spec->name,name,MAX_NAME_LEN);
     a_spec->name[MAX_NAME_LEN]='\0';
     if(username){
	  strncpy(a_spec->username,username,MAX_USERNAME_LEN);
	  a_spec->username[MAX_USERNAME_LEN]='\0';
     }
     else
	  a_spec->username[0]='\0';

     if(service){
	  strncpy(a_spec->servicename,service,255);
	  a_spec->servicename[255]='\0';
     }
     else
	  a_spec->servicename[0]='\0';

     a_spec->port=htons(port);

     haddr=(client_address->s_addr);
     hmask=(client_netmask->s_addr);
     a_spec->hclient_address=haddr;
     
     if(hmask!=0)
	  a_spec->hclient_netmask=hmask;
     else{
	  if(haddr!=0) 
	       a_spec->hclient_netmask=htonl(0xFFFFFFFF);
	  else
	       a_spec->hclient_netmask=hmask;
     }
     a_spec->hserver_address=(server_address->s_addr);

     if(acl_spec_list==NULL){
	  acl_spec_list=a_spec;
	  acl_spec_last=a_spec;
     }
     else{
	  acl_spec_last->next=a_spec;
	  acl_spec_last=a_spec;
     }
     debug_printf(10,"ACL spec name:%s username:%s port:%d src_ip:%s src_netmask:%s server_ip:%s  \n",
		  name,username,port,
		  inet_ntoa(*client_address),inet_ntoa(*client_netmask),inet_ntoa(*server_address));
     return a_spec;
}


/********************************************************************/
/*Configuration functions ...............                           */

int acl_add(char *directive,char **argv,void *setdata){
     char *name, *username,*service,*str;
     int i,res;    
     unsigned int port;
     struct in_addr client_address, client_netmask, server_address;
     username=NULL;
     service=NULL;
     port=0;
     client_address.s_addr=0;
     client_netmask.s_addr=0;
     server_address.s_addr=0;


     if(argv[0]==NULL || argv[1]==NULL){
	  debug_printf(1,"Parse error in directive %s \n",directive);
	  return 0;
     }
     name=argv[0];
     i=1;
     while(argv[i]!=NULL){
	  if(argv[i+1]==NULL){
	       debug_printf(1,"Parse error in directive %s \n",directive);
	       return 0;
	  }

	  if(strcmp(argv[i],"src")==0){ /*has the form ip/netmask */
	       if(str=strchr(argv[i+1],'/')){
		    *str='\0';
		    str=str+1;
		    if(!(res=inet_aton(str,&client_netmask))){
			 debug_printf(1,"Invalid src netmask address %s. Disabling %s acl spec \n",str,name);
			 return 0;
		    }

	       }
	       else{
		    inet_aton("255.255.255.255",&client_netmask);
	       }
	       if(!(res=inet_aton(argv[i+1],&client_address))){
		    debug_printf(1,"Invalid src ip address %s. Disabling %s acl spec \n",argv[i+1],name);
		    return 0;
	       }
	  }
	  else if(strcmp(argv[i],"srvip")==0){ /*has the form ip */
	       if(!(res=inet_aton(argv[i+1],&client_address))){
		    debug_printf(1,"Invalid server ip address %s. Disabling %s acl spec \n",argv[i+1],name);
		    return 0;
	       }
	  }
	  else if(strcmp(argv[i],"port")==0){ /*an integer */
	       if((port=strtol(argv[i+1],NULL,10))==0){
		    debug_printf(1,"Invalid server port  %s. Disabling %s acl spec \n",argv[i+1],name);
		    return 0;
	       }
	  }
	  else if(strcmp(argv[i],"user")==0){ /*a string*/
	       username=argv[i+1];
	  }
	  else if(strcmp(argv[i],"service")==0){ /*a string*/
	       service=argv[i+1];
	  }
	  else{
	       debug_printf(1,"Invalid directive :%s. Disabling %s acl spec \n",argv[i],name);
	       return 0;
	  }
	  i+=2;
     }
     
     new_acl_spec(name,username,port,service,&client_address,&client_netmask,&server_address);
     
     return 1;
}


int acl_access(char *directive,char **argv,void *setdata){
     int type;
     char *acl_spec;

     if(argv[0]==NULL || argv[1]==NULL){
	  debug_printf(1,"Parse error in directive %s \n",directive);
	  return 0;
     }
     if(strcmp(argv[0],"allow")==0){
	  type=CI_ACCESS_ALLOW;
     }
     else if(strcmp(argv[0],"deny")==0){
	  type=CI_ACCESS_DENY;
     }
     else if(strcmp(argv[0],"http_auth")==0){
	  type=CI_ACCESS_HTTP_AUTH;
     }
     else{
	  debug_printf(1,"Invalid directive :%s. Disabling %s acl rule \n",argv[0],argv[1]);
	  return 0;
     }
     acl_spec=argv[1];
     
     new_access_entry(type,acl_spec);
}


