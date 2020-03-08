/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:
    dragino-fwd, sqlite3 db.c

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "utilities.h"
#include "db.h"

#define DEBUG_SQL  0

#define DEBUG_STMT(stmt) if (DEBUG_SQL) { sql_debug(stmt);} 

static void sql_debug(sqlite3_stmt* stmt);

static bool db_createtb(struct context* cntx);
static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data);

static bool db_createtb(struct context* cntx) {
    sqlite3_stmt* createabpdevs = NULL;
    sqlite3_stmt* inserttskey = NULL;

    INITSTMT(CREATEABPDEVS, createabpdevs);
    
    if (!db_step(createabpdevs, NULL, NULL)) {
        printf("error running table create statement");
        sqlite3_finalize(createabpdevs);
        goto out;
    }
    sqlite3_finalize(createabpdevs);

    return true;
out:
    return false;
}

bool db_init(const char* dbpath, struct context* cntx) {
    int ret;
    ret = sqlite3_open(dbpath, &cntx->db);
    if (ret) {
    printf("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->db));
	sqlite3_close(cntx->db);
	return false;
    }

    if (!db_createtb(cntx))
        goto out;

    INITSTMT(LOOKUPSKEY, cntx->lookupskey);

    return true;

out:    
    printf("GOTO out!\n");
    sqlite3_finalize(cntx->lookupskey);
    sqlite3_close(cntx->db);
    return false;

}

void db_destroy(struct context* cntx) {
    sqlite3_finalize(cntx->lookupskey);
    sqlite3_close(cntx->db);
}

bool db_lookup_skey(sqlite3_stmt* stmt, void* data) {
    int ret;
    char devaddr[16] = {'\0'};
    struct devinfo* devinfo = (struct devinfo*) data;
    snprintf(devaddr, sizeof(devaddr), "%08X", devinfo->devaddr);
    sqlite3_bind_text(stmt, 1, devaddr, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW) {
        str2hex(devinfo->appskey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->appskey));
        str2hex(devinfo->nwkskey, (char *)sqlite3_column_text(stmt, 1), sizeof(devinfo->nwkskey));
        sqlite3_reset(stmt);
        return true;
    }
    sqlite3_reset(stmt);
    return false;
}

static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data) {
    int ret;
    DEBUG_STMT(stmt);
    while (1) {
	ret = sqlite3_step(stmt);
	if (ret == SQLITE_DONE) {
	    return true;
    } else if (ret == SQLITE_ROW) {
	if (rowcallback != NULL)
	    rowcallback(stmt, data);
	} else {
	    printf("ERROR~ %s\n", sqlite3_errstr(ret));
	    return false;
	}
    }
}

static void sql_debug(sqlite3_stmt* stmt) { 
    char *sql;
    sql = sqlite3_expanded_sql(stmt);
    printf("\nDEBUG-DB~ SQL=(%s)\n", sql);
    sqlite3_free(sql);
}

