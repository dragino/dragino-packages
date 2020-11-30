/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief Persistant data storage 
 */

#ifndef _LGW_DB_H
#define _LGW_DB_H

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define LGW_DB_FILE        "/tmp/lgwdb.sqlite"

struct lgw_db_entry {
	struct lgw_db_entry *next;
	char *key;
	char data[0];
};

/*! \brief initial database */
int lgw_db_init(void);

/*! \brief find key value specified by family/key */
bool lgw_db_key_exist(const char *key);

/*! \brief Get key value specified by family/key */
int lgw_db_get(const char *family, const char *key, char *value, int valuelen);

/*!
 * \brief Get key value specified by family/key as a heap allocated string.
 *
 * \details
 * Given a \a family and \a key, sets \a out to a pointer to a heap
 * allocated string.  In the event of an error, \a out will be set to
 * NULL.  The string must be freed by calling lgw_free().
 *
 * \retval -1 An error occurred
 * \retval 0 Success
 */
int lgw_db_get_allocated(const char *family, const char *key, char **out);

/*! \brief Store value addressed by family/key */
int lgw_db_put(const char *family, const char *key, const char *value);

/*! \brief Store value on table livepkts */
int lgw_db_putpkt(char* pdtype, double freq, char* dr, uint16_t cnt, char* devaddr, char* content, char* payload);

/*! \brief Delete entry in gwdb */
int lgw_db_del(const char *family, const char *key);

/*!
 * \brief Delete one or more entries in gwdb
 *
 * \details
 * If both parameters are NULL, the entire database will be purged.  If
 * only keytree is NULL, all entries within the family will be purged.
 * It is an error for keytree to have a value when family is NULL.
 *
 * \retval -1 An error occurred
 * \retval >= 0 Number of records deleted
 */
int lgw_db_deltree(const char *family, const char *keytree);

/*!
 * \brief Get a list of values within the gwdb tree
 *
 * \details
 * If family is specified, only those keys will be returned.  If keytree
 * is specified, subkeys are expected to exist (separated from the key with
 * a slash).  If subkeys do not exist and keytree is specified, the tree will
 * consist of either a single entry or NULL will be returned.
 *
 * Resulting tree should be freed by passing the return value to lgw_db_freetree()
 * when usage is concluded.
 */
struct lgw_db_entry *lgw_db_gettree(const char *family, const char *keytree);

/*!
 * \brief Get a list of values with the given key prefix
 *
 * \param family The family to search under
 * \param key_prefix The key prefix to search under
 *
 * \retval NULL An error occurred
 */
struct lgw_db_entry *lgw_db_gettree_by_prefix(const char *family, const char *key_prefix);

/*! \brief Free structure created by lgw_db_gettree() */
void lgw_db_freetree(struct lgw_db_entry *entry);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _LGW_DB_H */
