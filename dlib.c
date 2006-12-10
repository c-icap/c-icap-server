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
#include "dlib.h"


struct dlib_entry{
     char *file;
     char *name;
     CI_DLIB_HANDLE handle;
     struct dlib_entry *next;
};

struct dlib_entry *dlib_list = NULL;


int ci_dlib_entry(char *name,char *file, CI_DLIB_HANDLE handle){
     struct dlib_entry *dl_e,*dl_cur;

     if(!name || ! file || !handle)
	  return 0;
     dl_e=malloc(sizeof(struct dlib_entry));
     if(!dl_e)
	  return 0;
     dl_e->file=strdup(file);
     if(!dl_e->file){
	  free(dl_e);
	  return 0;
     }
     dl_e->name=strdup(name);
     if(!dl_e->name){
	  free(dl_e->file);
	  free(dl_e);
	  return 0;
     }
     dl_e->handle=handle;
     dl_e->next=NULL;

     if(dlib_list==NULL){
	  dlib_list=dl_e;
	  return 1;
     }
     dl_cur=dlib_list;
     while(dl_cur->next!=NULL)
	  dl_cur=dl_cur->next;

     dl_cur->next=dl_e;
     return 1;
}



int ci_dlib_closeall(){
     struct dlib_entry *dl_e,*dl_cur;
     int ret,error=0;
     dl_cur=dlib_list;
     while(dl_cur!=NULL){
	  dl_e=dl_cur;
	  dl_cur=dl_cur->next;	  
	  ret=ci_module_unload(dl_e->handle,dl_e->name);
	  if(!ret)
	       error=1;
	  if(dl_e->name)
	       free(dl_e->name);
	  if(dl_e->file)
	       free(dl_e->file);
	  free(dl_e);
     }
     if(error) return 0;
     return 1;
}
