/*
 * for sqlite3 databases 
 * db.h 
 *
 * Author: skerlan
 */

#ifndef DB_SQLITE3_H_
#define DB_SQLITE3_H_

#include <sqlite3.h>

#define LOOKUPSKEY "select appskey, nwkskey from abpdevs where devaddr = ?;"

#define CREATEABPDEVS "\
CREATE TABLE IF NOT EXISTS `abpdevs` (\
  `devaddr` TEXT UNIQUE,\
  `appskey` TEXT,\
  `nwkskey` TEXT\
);"

#define INSERTTSKEY "INSERT OR IGNORE INTO abpdevs (devaddr, appskey, nwkskey) VALUES ('2604185D', '850654CCC7614CFCCFA138BDFC285E82', '5DE37E916757759FCF81CA684C253E35')"

#define INITSTMT(SQL, STMT) if (sqlite3_prepare_v2(cntx->db, SQL, -1, &STMT, NULL) != SQLITE_OK) {  \
			    printf("failed to prepare sql; %s -> %s\n", SQL,  sqlite3_errmsg(cntx->db));\
			    goto out;\
			}

struct context {
    sqlite3* db;
    sqlite3_stmt* lookupskey;
};

struct devinfo {
    uint32_t devaddr;
    uint8_t appskey[16];
    uint8_t nwkskey[16];

    /* hex string
    uint8_t appskey_hex[33];
    uint8_t nwkskey_hex[33];
    */
};

bool db_init(const char* dbpath, struct context* cntx);
void db_destroy(struct context* cntx);
bool db_lookup_skey(sqlite3_stmt* stmt, void* data);

#endif   /* end defined DB_SQLITE3_H_ */
