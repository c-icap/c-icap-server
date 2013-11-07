/*
 *  Copyright (C) 2004-2010 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */

#include "common.h"
#include "c-icap.h"
#include "net_io.h"
#include "mem.h"
#include "lookup_table.h"
#include "cfg_param.h"
#include "filetype.h"
#include "debug.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

/*string operators */
void *stringdup(const char *str, ci_mem_allocator_t *allocator)
{
    char *new_s = allocator->alloc(allocator,strlen(str)+1);
    if(new_s)
      strcpy(new_s, str);
    return new_s;
}

int stringcmp(const void *key1,const void *key2)
{
    if (!key2)
        return -1;
    return strcmp((const char *)key1,(const char *)key2);
}

int stringequal(const void *key1,const void *key2)
{
    if (!key2)
        return 0;
    return strcmp((const char *)key1,(const char *)key2)==0;
}

size_t stringlen(const void *key)
{
    return strlen((const char *)key)+1;
}

void stringfree(void *key, ci_mem_allocator_t *allocator)
{
    allocator->free(allocator, key);
}

const ci_type_ops_t  ci_str_ops = {
    stringdup,
    stringfree,
    stringcmp,
    stringlen,
    stringequal,
};

int string_ext_cmp(const void *key1,const void *key2)
{
    if (!key2)
        return -1;

    if (strcmp((const char *)key1, "*") == 0)
	return 0;

    return strcmp((const char *)key1,(const char *)key2);
}

int string_ext_equal(const void *key1,const void *key2)
{
    if (!key2)
        return 0;

    if (strcmp((const char *)key1, "*") == 0)
	return 1;

    return strcmp((const char *)key1,(const char *)key2)==0;
}


const ci_type_ops_t ci_str_ext_ops = {
    stringdup,
    stringfree,
    string_ext_cmp,
    stringlen,
    string_ext_equal,
};


/*int32 operators*/

void *int32_dup(const char *str, ci_mem_allocator_t *allocator)
{
    int32_t *i;
    i = allocator->alloc(allocator, sizeof(int32_t));
    if (i)
        *i = strtol(str, NULL, 10);    
    return (void *)i;

}

int int32_cmp(const void *key1,const void *key2)
{
    int32_t k1, k2;
    k1 = *(int32_t *)key1;
    k2 = *(int32_t *)key2;
    if (k1 < k2)
        return -1;
    if (k1 > k2)
        return 1;

    return 0;
}

int int32_equal(const void *key1,const void *key2)
{
    int32_t k1,k2;
    k1 = *(int32_t *)key1;
    k2 = *(int32_t *)key2;
    return k1 == k2;
}

size_t int32_len(const void *key)
{
    return (size_t)4;
}

void int32_free(void *key, ci_mem_allocator_t *allocator)
{
    /*nothing*/
    allocator->free(allocator, key);
}

const ci_type_ops_t  ci_int32_ops = {
    int32_dup,
    int32_free,
    int32_cmp,
    int32_len,
    int32_equal
};

/*uint64 operators*/
void *uint64_dup(const char *str, ci_mem_allocator_t *allocator)
{
    uint64_t *i;
    i = allocator->alloc(allocator, sizeof(uint64_t));
    if (i)
        *i = strtoll(str, NULL, 10);    
    return (void *)i;
}

int uint64_cmp(const void *key1,const void *key2)
{
    uint64_t k1,k2;
    k1 = *(uint64_t *)key1;
    k2 = *(uint64_t *)key2;
    if (k1 < k2)
        return -1;
    if (k1 > k2)
        return 1;

    return 0;
}

int uint64_equal(const void *key1,const void *key2)
{
   uint64_t k1,k2;
    k1 = *(uint64_t *)key1;
    k2 = *(uint64_t *)key2;
    return k1 == k2;
}

void uint64_free(void *key, ci_mem_allocator_t *allocator)
{
    allocator->free(allocator, key);
}

size_t uint64_len(const void *key)
{
    return (size_t)sizeof(uint64_t);
}

const ci_type_ops_t  ci_uint64_ops = {
    uint64_dup,
    uint64_free,
    uint64_cmp,
    uint64_len,
    uint64_equal
};


/*regular expresion operator definition  */
#ifdef USE_REGEX
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
        s++;
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

int regex_cmp(const void *key1, const void *key2)
{
    regmatch_t pmatch[1];
    struct ci_regex *reg=(struct ci_regex *)key1;
    if (!key2)
        return -1;
    return regexec(&reg->preg, (char *)key2, 1, pmatch, 0);
}

int regex_equal(const void *key1, const void *key2)
{
    regmatch_t pmatch[1];
    struct ci_regex *reg=(struct ci_regex *)key1;
    if (!key2)
        return 0;
    return regexec(&reg->preg, (const char *)key2, 1, pmatch, 0)==0;
}

size_t regex_len(const void *key)
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


const ci_type_ops_t  ci_regex_ops = {
    regex_dup,
    regex_free,
    regex_cmp,
    regex_len,
    regex_equal
};
#endif

/*filetype operators*/
void *datatype_dup(const char *str, ci_mem_allocator_t *allocator)
{
    int type;
    unsigned int  *val = allocator->alloc(allocator,sizeof(unsigned int));
    if ((type = ci_magic_type_id(str)) >= 0) {
      *val = type;
    }
    else if ( (type = ci_magic_group_id(str)) >= 0) {
       *val = type;
       *val = *val << 16;
    }
    else {
         allocator->free(allocator, val);
         val = NULL;
    }
 
    return (void *)val;
}

int datatype_cmp(const void *key1, const void *key2)
{
    unsigned int type = *(unsigned int *)key1;

    if (!key2)
        return -1;

    if ( (0xFFFF0000 & type) == 0)
       return (*(unsigned int *)key1 - *(unsigned int *)key2);
    else { /*type is group check if key2 belongs to group*/
         type = type >> 16;
         if (ci_magic_group_check(*(unsigned int *)key2, type))
             return 0;
         else 
             return 1;
    }
}

int datatype_equal(const void *key1, const void *key2)
{
    unsigned int type = *(unsigned int *)key1;

    if (!key2)
        return 0;

    if ( (0xFFFF0000 & type) == 0)
        return *(unsigned int *)key1 == *(unsigned int *)key2;
    else { /*type is group check if key2 belongs to group*/
         type = type >> 16;
         if (ci_magic_group_check(*(unsigned int *)key2, (int)type))
             return 1;
         else
             return 0;
    }
}

size_t datatype_len(const void *key)
{
    return sizeof(unsigned int);
}

void datatype_free(void *key, ci_mem_allocator_t *allocator)
{
    /*nothing*/
     allocator->free(allocator, key);
}
   
const ci_type_ops_t  ci_datatype_ops = {
    datatype_dup,
    datatype_free,
    datatype_cmp,
    datatype_len,
    datatype_equal
};

/*IP operators*/
#ifdef HAVE_IPV6

void ci_list_ipv4_to_ipv6();

#define ci_ipv4_inaddr_is_zero(addr) ((addr).ipv4_addr.s_addr==0)
#define ci_ipv4_inaddr_are_equal(addr1,addr2) ((addr1).ipv4_addr.s_addr == (addr2).ipv4_addr.s_addr)
#define ci_ipv4_inaddr_zero(addr) ((addr).ipv4_addr.s_addr=0)

#define ci_ipv6_inaddr_is_zero(addr) ( ci_in6_addr_u32(addr)[0]==0 && \
				       ci_in6_addr_u32(addr)[1]==0 &&  \
				       ci_in6_addr_u32(addr)[2]==0 &&  \
				       ci_in6_addr_u32(addr)[3]==0)

#define ci_ipv6_inaddr_are_equal(addr1,addr2) ( ci_in6_addr_u32(addr1)[0]==ci_in6_addr_u32(addr2)[0] && \
						ci_in6_addr_u32(addr1)[1]==ci_in6_addr_u32(addr2)[1] && \
						ci_in6_addr_u32(addr1)[2]==ci_in6_addr_u32(addr2)[2] && \
						ci_in6_addr_u32(addr1)[3]==ci_in6_addr_u32(addr2)[3])


#define ci_ipv6_inaddr_is_v4mapped(addr) (ci_in6_addr_u32(addr)[0]==0 &&\
					  ci_in6_addr_u32(addr)[1]==0 && \
					  ci_in6_addr_u32(addr)[2]== htonl(0xFFFF))


#define ci_ipv4_inaddr_check_net(addr1,addr2,mask) (((addr1).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr)==((addr2).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr))
#define ci_ipv6_inaddr_check_net(addr1,addr2,mask) ((ci_in6_addr_u32(addr1)[0] & ci_in6_addr_u32(mask)[0])==(ci_in6_addr_u32(addr2)[0] & ci_in6_addr_u32(mask)[0]) &&\
						    (ci_in6_addr_u32(addr1)[1] & ci_in6_addr_u32(mask)[1])==(ci_in6_addr_u32(addr2)[1] & ci_in6_addr_u32(mask)[1]) && \
						    (ci_in6_addr_u32(addr1)[2] & ci_in6_addr_u32(mask)[2])==(ci_in6_addr_u32(addr2)[2] & ci_in6_addr_u32(mask)[2]) && \
						    (ci_in6_addr_u32(addr1)[3] & ci_in6_addr_u32(mask)[3])==(ci_in6_addr_u32(addr2)[3] & ci_in6_addr_u32(mask)[3]))
#define ci_ipv4_in_ipv6_check_net(addr1, addr2, mask) (ci_in6_addr_u32(addr2)[0]==0 && \
						       ci_in6_addr_u32(addr2)[1]==0 && \
						       ci_in6_addr_u32(addr2)[2]== htonl(0xFFFF) && \
						       ((addr1).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr)==(ci_in6_addr_u32(addr2)[3] & (mask).ipv4_addr.s_addr))
#define ci_ipv6_in_ipv4_check_net(addr1, addr2, mask) (ci_in6_addr_u32(addr1)[0]==0 && \
						       ci_in6_addr_u32(addr1)[1]==0 && \
						       ci_in6_addr_u32(addr1)[2]== htonl(0xFFFF) && \
						       (ci_in6_addr_u32(addr1)[3] & (mask).ipv4_addr.s_addr) == ((addr2).ipv4_addr.s_addr & (mask).ipv4_addr.s_addr))


/*We can do this because ipv4_addr in practice exists in s6_addr[0]*/
#define ci_inaddr_ipv4_to_ipv6(addr)( ci_in6_addr_u32(addr)[3]=(addr).ipv4_addr.s_addr,\
				      ci_in6_addr_u32(addr)[0]=0,	\
				      ci_in6_addr_u32(addr)[1]=0,	\
				      ci_in6_addr_u32(addr)[2]= htonl(0xFFFF))
#define ci_netmask_ipv4_to_ipv6(addr)(ci_in6_addr_u32(addr)[3]=(addr).ipv4_addr.s_addr,	\
				      ci_in6_addr_u32(addr)[0]= htonl(0xFFFFFFFF), \
				      ci_in6_addr_u32(addr)[1]= htonl(0xFFFFFFFF), \
				      ci_in6_addr_u32(addr)[2]= htonl(0xFFFFFFFF))
#else                           /*if no HAVE_IPV6 */

#define ci_ipv4_inaddr_is_zero(addr) ((addr).s_addr==0)
#define ci_ipv4_inaddr_are_equal(addr1,addr2) ((addr1).s_addr == (addr2).s_addr)
#define ci_ipv4_inaddr_check_net(addr1,addr2,mask) (((addr1).s_addr & (mask).s_addr)==((addr2).s_addr & (mask).s_addr))

#define ci_ipv4_inaddr_hostnetmask(addr)((addr).s_addr=htonl(0xFFFFFFFF))
#define ci_ipv4_inaddr_zero(addr) ((addr).s_addr=0)

#endif                          /*ifdef HAVE_IPV6 */




void *ip_dup(const char *value,  ci_mem_allocator_t *allocator){
    int socket_family, len;
    ci_ip_t *ip;
    char str_addr[CI_IPLEN+1], str_netmask[CI_IPLEN+1];
    char *pstr;
    ci_in_addr_t address, netmask;

    ci_inaddr_zero(address);
    ci_inaddr_zero(netmask);

#ifdef HAVE_IPV6
    if(strchr(value,':'))
	socket_family = AF_INET6;
    else
#endif
	socket_family = AF_INET;

    if ((pstr=strchr(value,'/'))){
	len=(pstr-value);
	if (len >= CI_IPLEN){
	    ci_debug_printf(1,"Invalid ip address (len>%d): %s\n", CI_IPLEN, value);
	    return NULL;
	}
	strncpy(str_addr,value,len);
	str_addr[len] = '\0';

	if(!ci_inet_aton(socket_family, str_addr, &address)){
	    ci_debug_printf(1,"Invalid ip address in network %s definition\n", value);
	    return NULL;
	}
	
	strncpy(str_netmask, pstr+1, CI_IPLEN);
	str_netmask[CI_IPLEN] = '\0';

	if(!ci_inet_aton(socket_family, str_netmask, &netmask)){
	    ci_debug_printf(1,"Invalid netmask in network %s definition\n", value);
	    return NULL;
	}
    }
    else { /*No netmask defined is a host ip*/
	if(!ci_inet_aton(socket_family, value, &address)){
	    ci_debug_printf(1,"Invalid ip address: %s\n", value);
	    return NULL;
	}
#ifdef HAVE_IPV6
	if(socket_family==AF_INET)
	    ci_ipv4_inaddr_hostnetmask(netmask);
	else
	    ci_ipv6_inaddr_hostnetmask(netmask);
#else
	ci_ipv4_inaddr_hostnetmask(netmask);
#endif
    }
    
    ip= allocator->alloc(allocator, sizeof(ci_ip_t));
    ip->family = socket_family;
    
    ci_inaddr_copy(ip->address, address);
    ci_inaddr_copy(ip->netmask, netmask); 
    
    return ip;
}

void ip_free(void *data, ci_mem_allocator_t *allocator) {
    allocator->free(allocator, data);
}

size_t ip_len(const void *key)
{
    return sizeof(ci_ip_t);
}

int ip_cmp(const void *ref_key, const void *key_check) {
    /*Not implemented*/
    return 0;
}

int ip_equal(const void *ref_key, const void *key_check) {
    const ci_ip_t *ip_ref = (const ci_ip_t *)ref_key;
    const ci_ip_t *ip_check = (const ci_ip_t *)key_check;
    char buf[128],buf1[128],buf2[128];

    if (!ip_check)
        return 0;

    ci_debug_printf(9,"going to check addresses  ip address: %s %s/%s\n",
		    ci_inet_ntoa(ip_check->family,&ip_check->address, buf, 128),
		    ci_inet_ntoa(ip_ref->family,&ip_ref->address, buf1, 128),
		    ci_inet_ntoa(ip_ref->family,&ip_ref->netmask, buf2, 128)
	);
#ifdef HAVE_IPV6
    if(ip_check->family == AF_INET){
	if(ip_ref->family == AF_INET)
	    return ci_ipv4_inaddr_check_net(ip_ref->address, ip_check->address, ip_ref->netmask);
	//else add->family == AF_INET6
	return ci_ipv6_in_ipv4_check_net(ip_ref->address, ip_check->address, ip_ref->netmask);
    }
    //else assuming  ip_check->family == AF_INET6 
    if(ip_ref->family == AF_INET6)
	return ci_ipv6_inaddr_check_net(ip_ref->address, ip_check->address, ip_ref->netmask);
    //else ip->family == AF_INET
    return ci_ipv4_in_ipv6_check_net(ip_ref->address, ip_check->address, ip_ref->netmask);
#else
    return ci_ipv4_inaddr_check_net(ip_ref->address, ip_check->address, ip_ref->netmask);
#endif

}

int ip_sockaddr_cmp(const void *ref_key, const void *key_check) {
    /*Not implemented*/
    return 1;
}

int ip_sockaddr_equal(const void *ref_key, const void *key_check) {
    const ci_ip_t *ip_ref = (const ci_ip_t *)ref_key;
    const ci_sockaddr_t *ip_check = (const ci_sockaddr_t *)key_check;
    char buf[128],buf1[128],buf2[128];

    if (!ip_check)
        return 0;

    ci_debug_printf(9,"going to check addresses  ip address: %s %s/%s\n",
		    ci_inet_ntoa(ip_check->ci_sin_family,ip_check->ci_sin_addr, buf, 128),
		    ci_inet_ntoa(ip_ref->family,&ip_ref->address, buf1, 128),
		    ci_inet_ntoa(ip_ref->family,&ip_ref->netmask, buf2, 128)
	);
#ifdef HAVE_IPV6
    if(ip_check->ci_sin_family == AF_INET){
	if(ip_ref->family == AF_INET)
	    return ci_ipv4_inaddr_check_net(ip_ref->address, *(ci_in_addr_t *)ip_check->ci_sin_addr, ip_ref->netmask);
	//else add->family == AF_INET6
	return ci_ipv6_in_ipv4_check_net(ip_ref->address, *(ci_in_addr_t *)ip_check->ci_sin_addr, ip_ref->netmask);
    }
    //else assuming  ip_check->ci_sin_family == AF_INET6 
    if(ip_ref->family == AF_INET6)
	return ci_ipv6_inaddr_check_net(ip_ref->address, *(ci_in_addr_t *)ip_check->ci_sin_addr, ip_ref->netmask);
    //else ip->family == AF_INET
    return ci_ipv4_in_ipv6_check_net(ip_ref->address, *(ci_in_addr_t *)ip_check->ci_sin_addr, ip_ref->netmask);
#else
    return ci_ipv4_inaddr_check_net(ip_ref->address, *(ci_in_addr_t *)ip_check->ci_sin_addr, ip_ref->netmask);
#endif

}



const ci_type_ops_t  ci_ip_ops = {
    ip_dup,
    ip_free,
    ip_cmp,
    ip_len,
    ip_equal
};



const ci_type_ops_t ci_ip_sockaddr_ops = {
    ip_dup,
    ip_free,
    ip_sockaddr_cmp,
    ip_len,
    ip_sockaddr_equal
};
