/*
 *  Copyright (C) 2004-2009 Christos Tsantilas
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

#ifndef __ACL_H
#define __ACL_H

#include "c-icap.h"
#include "net_io.h"
#include "types_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 \defgroup ACL Access lists API
 \ingroup API
 * Access control lists related API. Structures, functions and macros used to define
 * custom acl types, and use access control lists in services and modules.
 */

#define MAX_NAME_LEN 31  

/*ACL type structures and functions */
struct ci_request;
/**
   \ingroup ACL
   * This is the struct used to implement an acl type object.
 */
typedef struct ci_acl_type{

    /**
       \brief The acl type name
     */
     char name[MAX_NAME_LEN+1];

    /**
       \brief Pointer to the functions which retrieves the test data for this acl type
       *
       * This method extract the test data from request object for this acl object.
       * For example for the "src" acl type this function will extract the icap client 
       * ip address
       \param req Pointer to the related ci_request_t object 
       \param param Some acl types supports one parameter passed by the c-icap administrator
       \return A pointer to the test data
     */
     void *(*get_test_data)(struct ci_request *req, char *param); 

    /**
       \brief Pointer to the function which release the acl test data (if required)
       *
       * This method releases the acl test data,which  allocated using the get_test_data method
       \param req Pointer to the related ci_request_t object
       \param data Pointer to allocated test data
     */
     void (*free_test_data)(struct ci_request *req, void *data);

    /**
       \brief Pointer to the ci_types_ops_t struct which implements basic operations for the acl test data
     */
     ci_type_ops_t *type;
} ci_acl_type_t;

struct ci_acl_type_list{
     ci_acl_type_t *acl_type_list;
     int acl_type_list_size;
     int acl_type_list_num;
};

int ci_acl_typelist_init(struct ci_acl_type_list *list);
int ci_acl_typelist_add(struct ci_acl_type_list *list, const ci_acl_type_t *type);
int ci_acl_typelist_release(struct ci_acl_type_list *list);
int ci_acl_typelist_reset(struct ci_acl_type_list *list);
const ci_acl_type_t *ci_acl_typelist_search(struct ci_acl_type_list *list, const char *name);


/*ACL specs structures and functions */

typedef struct ci_acl_data ci_acl_data_t;
struct ci_acl_data{
     void *data;
     ci_acl_data_t *next;
};

/**
   \brief This struct holds an access control list (acl).
   \ingroup ACL
   *
   * Imagine the following access control list defined in the c-icap config file:\n
   * \code acl LOCALNET 127.0.0.1/255.255.255.255 192.168.1.0/255.255.255.0
   * \endcode
   * This struct represents access control lists like the above
 */
typedef struct ci_acl_spec ci_acl_spec_t;
struct ci_acl_spec {
     char name[MAX_NAME_LEN + 1];
     const ci_acl_type_t *type;
     char *parameter;
     ci_acl_data_t *data;
     ci_acl_spec_t *next;
};

/*Specs lists and access entries structures and functions */
typedef struct ci_specs_list ci_specs_list_t;
struct ci_specs_list{
     const ci_acl_spec_t *spec;
     int negate;
     ci_specs_list_t *next;
};

/**
   \brief An access entry object holds an access control list, and can be connected to linked lists of access entries.
   \ingroup ACL
   *
   * This struct used to implement lists of access control lists.
   * Each access entry can hold an "allow" or "deny" access control list.
   * An access entries list represents the following c-icap config lines:
   * \code 
   *  icap_access allow LOCALNET LOCALHOST
   *  icap_access deny ALL
   * \endcode
   * each of the above lines represented by an ci_access_entry object.
   * The ci_access_entry objects can be connected to a simple linked list.
   *
 */
typedef struct ci_access_entry ci_access_entry_t;
struct ci_access_entry {
     int type;   /*CI_ACCESS_DENY or CI_ACCESS_ALLOW*/
     ci_specs_list_t *spec_list;
     ci_access_entry_t *next;
};

/**
   \brief Append a new access entry object to an access entries list
   \ingroup ACL
   *
   \param list Pointer to the access entry list
   \param type CI_ACCESS_ALLOW if CI_ACCESS_DENY to specify if this access entry holds an "allow" or "deny" access control list
   \return A pointer to the newly created access entry object
 */
CI_DECLARE_FUNC(ci_access_entry_t *) ci_access_entry_new(ci_access_entry_t **list, int type);

/**
   \brief Destroy an access entries list
   \ingroup ACL
   *
   \param list Pointer to the access entries list
*/
CI_DECLARE_FUNC(void) ci_access_entry_release(ci_access_entry_t *list);

CI_DECLARE_FUNC(const ci_acl_spec_t *) ci_access_entry_add_acl(ci_access_entry_t *access_entry, const ci_acl_spec_t *acl, int negate);

/**
   \brief Add an acl to an access entry object
   \ingroup ACL
   *
   \param access_entry Pointer to the access entry  object
   \param aclname The name of the acl to be added.
   \return non zero on success, zero otherwise
 */
CI_DECLARE_FUNC(int) ci_access_entry_add_acl_by_name(ci_access_entry_t *access_entry, const char *aclname);

/**
   \brief Check if an access entries list matches a request object
   \ingroup ACL
   *
   \param access_entry Pointer to the access entries list (a linked list of ci_access_entry_t objects)
   \param req pointer to the request object (ci_request_t object)
   \return CI_ACCESS_ALLOW if request matches the access list, CI_ACCESS_DENY otherwise
 */
CI_DECLARE_FUNC(int) ci_access_entry_match_request(ci_access_entry_t *access_entry, ci_request_t *req);


/*Inititalizing, reseting and tools acl library functions */

/**
   \brief Initializes the c-icap acl subsystem. It is not thread safe
   \ingroup ACL
 */
CI_DECLARE_FUNC(void) ci_acl_init();

/**
   \brief Resets the c-icap acl subsystem. It is not thread safe
   \ingroup ACL
 */
CI_DECLARE_FUNC(void) ci_acl_reset();

CI_DECLARE_FUNC(const ci_acl_spec_t *) ci_acl_search(const char *name);
CI_DECLARE_FUNC(int) ci_acl_add_data(const char *name, const char *type, const char *data);

/**
   \brief Search for an acl type
   \ingroup ACL
   *
   \param name The name of the acl type
   \return Pointer to the ci_acl_type_t structure which implements the acl type, or NULL
 */
CI_DECLARE_FUNC(const ci_acl_type_t *) ci_acl_type_search(const char *name);

/**
   \brief Add a custom acl type to the c-icap acl subsystem
   \ingroup ACL
   *
   \param type Pointer to the c-acl_type_t struct which implements the acl type
   \return non zero on success, zero otherwise
 */
CI_DECLARE_FUNC(int) ci_acl_type_add(const ci_acl_type_t *type);

#ifdef __cplusplus
}
#endif

#endif/* __ACL_H*/
