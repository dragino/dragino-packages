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
    bool ret;
    sqlite3_stmt* createabpdevs = NULL;
    sqlite3_stmt* createtotalpkt = NULL;

    INITSTMT(CREATEABPDEVS, createabpdevs);
    INITSTMT(CREATETOTALPKT, createtotalpkt);
    
    ret = db_step(createabpdevs, NULL, NULL);

    sqlite3_finalize(createabpdevs);

    ret |= db_step(createtotalpkt, NULL, NULL);

    sqlite3_finalize(createtotalpkt);

out:
    return ret;
}

bool db_init(const char* dbpath, struct context* cntx) {
    int ret;
    sqlite3_stmt* initpkt_stmt;
    sqlite3_stmt* delpkt_stmt;

    ret = sqlite3_open(dbpath, &cntx->db);
    if (ret) {
        printf("DBDEBUG~ Can't open database: %s\n", sqlite3_errmsg(cntx->db));
        sqlite3_close(cntx->db);
        return false;
    }

    if (!db_createtb(cntx))
        goto out;

    INITSTMT(LOOKUPSKEY, cntx->lookupskey);
    INITSTMT(INCTATOLUP, cntx->totalup_stmt);
    INITSTMT(INCTATOLDW, cntx->totaldw_stmt);

    INITSTMT("delete from totalpkt;", delpkt_stmt);
    INITSTMT("insert into totalpkt values (time('now', 'localtime'), 0, 0);", initpkt_stmt);

    ret = sqlite3_step(delpkt_stmt);
    if (ret != SQLITE_DONE) 
        printf("DBDEBUG~ delete from tatalpkt: %s \n", sqlite3_errstr(ret));
    sqlite3_finalize(delpkt_stmt);

    ret = sqlite3_step(initpkt_stmt);  // insert the first row of totalpkt table
    if (ret != SQLITE_DONE) 
        printf("DBDEBUG~ delete from tatalpkt: %s \n", sqlite3_errstr(ret));
    sqlite3_finalize(initpkt_stmt);

    return true;

out:    
    printf("DBDEBUG~ EXIT sqlite!\n");
    sqlite3_close(cntx->db);
    return false;
}

void db_destroy(struct context* cntx) {
    sqlite3_finalize(cntx->lookupskey);
    sqlite3_finalize(cntx->totalup_stmt);
    sqlite3_finalize(cntx->totaldw_stmt);
    sqlite3_close(cntx->db);
}

bool db_incpkt(sqlite3_stmt* stmt, int data) {
    bool ret;
    sqlite3_bind_int(stmt, 1, data);
    DEBUG_STMT(stmt);
    ret = db_step(stmt, NULL, NULL);
    sqlite3_reset(stmt);
    return ret;
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
            printf("DBDEBUG~ %s\n", sqlite3_errstr(ret));
            return false;
        }
    }
}

static void sql_debug(sqlite3_stmt* stmt) { 
    char *sql;
    sql = sqlite3_expanded_sql(stmt);
    printf("\nDBDEBUG~ SQL=(%s)\n", sql);
    sqlite3_free(sql);
}

