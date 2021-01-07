/*
 * for sqlite3 databases 
 * db.h 
 *
 * Author: skerlan
 */

#ifndef DB_SQLITE3_H_
#define DB_SQLITE3_H_

#include <sqlite3.h>

#define LOOKUPSKEY "select appskey, nwkskey from KEY.abpdevs where devaddr = ?;"
#define INCTATOLUP "update totalpkt set totalup = ?;"
#define INCTATOLDW "update totalpkt set totaldw = ?;"

#define CREATEABPDEVS "\
CREATE TABLE IF NOT EXISTS `abpdevs` (\
  `devaddr` TEXT PRIMARY KEY NOT NULL,\
  `appskey` TEXT NOT NULL,\
  `nwkskey` TEXT NOT NULL\
);"

#define CREATEDWLINK "\
CREATE TABLE IF NOT EXISTS `dwlink` (\
  `id`  INTEGER PRIMARY KEY AUTOINCREMENT,\
  `devaddr` TEXT,\
  `data` TEXT,\
  `status` INTEGER NOT NULL DEFAULT 0\
);"

#define CREATETOTALPKT "\
CREATE TABLE IF NOT EXISTS `totalpkt` (\
  `startup` TEXT NOT NULL DEFAULT '0',\
  `totalup` INTEGER NOT NULL DEFAULT 0,\
  `totaldw` INTEGER NOT NULL DEFAULT 0\
);"

#define INITSTMT(SQL, STMT) if (sqlite3_prepare_v2(cntx->db, SQL, -1, &STMT, NULL) != SQLITE_OK) {  \
			    printf("failed to prepare sql; %s -> %s\n", SQL,  sqlite3_errmsg(cntx->db));\
			    goto out;\
			}

struct context {
    sqlite3* db;
    sqlite3_stmt* lookupskey;
    sqlite3_stmt* totalup_stmt;
    sqlite3_stmt* totaldw_stmt;
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
bool db_incpkt(sqlite3_stmt* stmt, int data);

#endif   /* end defined DB_SQLITE3_H_ */
