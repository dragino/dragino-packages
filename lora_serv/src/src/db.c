/*
 * for sqlite3 databases 
 * db.c
 *
 *  Created on: Feb 17, 2019
 *      Author: skerlan
 */

#include <string.h>
#include <stdbool.h>
#include "handle.h"
#include "utilities.h"
#include "db.h"

#define DEBUG_STMT(stmt) if (DEBUG_SQL) { sql_debug(stmt);} 

static void sql_debug(sqlite3_stmt* stmt);

static void lookup_appkey(sqlite3_stmt* stmt, void* data);
static void lookup_nwkskey(sqlite3_stmt* stmt, void* data);
static void judge_devaddr(sqlite3_stmt* stmt, void* data);
static bool db_createtb(struct context* cntx);
static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data);

static bool db_createtb(struct context* cntx) {
    int i;
    sqlite3_stmt* createdevs = NULL;
    sqlite3_stmt* creategws = NULL;
    sqlite3_stmt* createapps = NULL;
    sqlite3_stmt* createupmsg = NULL;
    sqlite3_stmt* creategwprofile = NULL;

    sqlite3_stmt* insertgws = NULL;
    sqlite3_stmt* insertapps = NULL;
    sqlite3_stmt* insertdevs = NULL;
    sqlite3_stmt* insertgwprofile = NULL;

    INITSTMT(CREATEDEVS, createdevs);
    INITSTMT(CREATEGWS, creategws);
    INITSTMT(CREATEAPPS, createapps);
    INITSTMT(CREATEUPMSG, createupmsg);
    INITSTMT(CREATEGWPROFILE, creategwprofile);


    sqlite3_stmt* createstmts[] = { createdevs, creategws, createapps, createupmsg, creategwprofile };

	for (i = 0; i < sizeof(createstmts)/sizeof(createstmts[0]); i++) {
        if (!db_step(createstmts[i], NULL, NULL)) {
            MSG("error running table create statement %d", i);
            sqlite3_finalize(createstmts[i]);
            goto out;
        }
        sqlite3_finalize(createstmts[i]);
    }

    INITSTMT(INSERTGWS, insertgws);
    INITSTMT(INSERTAPPS, insertapps);
    INITSTMT(INSERTDEVS, insertdevs);
    INITSTMT(INSERTGWPROFILE, insertgwprofile);

    sqlite3_stmt* insertstmts[] = { insertgws, insertapps, insertdevs, insertgwprofile };

	for (i = 0; i < sizeof(insertstmts)/sizeof(insertstmts[0]); i++) {
        if (!db_step(insertstmts[i], NULL, NULL)) {
            MSG("error running table create statement %d", i);
            sqlite3_finalize(insertstmts[i]);
            goto out;
        }
        sqlite3_finalize(insertstmts[i]);
    }

    return true;
out:
    return false;
}

bool db_init(const char* dbpath, struct context* cntx) {
    int i, ret;
	sqlite3_stmt* stmts[] = { cntx->lookupgweui, cntx->judgejoinrepeat, cntx->lookupappkey,
                              cntx->updatedevinfo, cntx->insertupmsg, cntx->judgedevaddr,
                              cntx->judgejoinrepeat, cntx->lookupnwkskey, cntx->lookupprofile };

	ret = sqlite3_open(dbpath, &cntx->db);
	if (ret) {
        printf("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->db));
	    sqlite3_close(cntx->db);
		return false;
	}

    if (!db_createtb(cntx))
        goto out;

    INITSTMT(LOOKUPGWEUI, cntx->lookupgweui);
    INITSTMT(JUDGEJOINREPEAT, cntx->judgejoinrepeat);
    INITSTMT(LOOKUPAPPKEY, cntx->lookupappkey);
    INITSTMT(UPDATEDEVINFO, cntx->updatedevinfo);
    INITSTMT(INSERTUPMSG, cntx->insertupmsg);
    INITSTMT(JUDGEDEVADDR, cntx->judgedevaddr);
    INITSTMT(JUDGEMSGREPEAT, cntx->judgemsgrepeat);
    INITSTMT(LOOKUPNWKSKEY, cntx->lookupnwkskey);
    INITSTMT(LOOKUPPROFILE, cntx->lookupprofile);

	return true;

out:    
	for (i = 0; i < sizeof(stmts)/sizeof(stmts[0]); i++) {
		sqlite3_finalize(stmts[i]);
	}

	sqlite3_close(cntx->db);
    return false;

}

void db_destroy(struct context* cntx) {
		sqlite3_finalize(cntx->lookupgweui);
		sqlite3_finalize(cntx->judgejoinrepeat);
		sqlite3_finalize(cntx->lookupappkey);
		sqlite3_finalize(cntx->updatedevinfo);
		sqlite3_finalize(cntx->insertupmsg);
		sqlite3_finalize(cntx->judgedevaddr);
		sqlite3_finalize(cntx->judgemsgrepeat);
		sqlite3_finalize(cntx->lookupnwkskey);
		sqlite3_finalize(cntx->lookupprofile);
		sqlite3_close(cntx->db);
}

bool db_lookup_gweui(sqlite3_stmt* stmt, char *gweui) {
	sqlite3_bind_text(stmt, 1, gweui, -1, SQLITE_STATIC);
	int ret = sqlite3_step(stmt);
    sqlite3_reset(stmt);
	if (ret == SQLITE_ROW) {
        return true;  /* have a gw register */
	} 
    return false;
}

bool db_judge_joinrepeat(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, devinfo->devnonce);
    DEBUG_STMT(stmt);
	int ret = sqlite3_step(stmt);
    sqlite3_reset(stmt);
	if (ret == SQLITE_ROW) {
        return true;  /* have a repeat devnonce*/
	} 
    return false;
}

bool db_lookup_appkey(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->appeui_hex, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
    bool ret = db_step(stmt, lookup_appkey, devinfo);
    sqlite3_reset(stmt);
    return ret;
}

bool db_update_devinfo(sqlite3_stmt* stmt, void* data) {
    bool ret;
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_int(stmt, 1, devinfo->devnonce);
	sqlite3_bind_int(stmt, 2, devinfo->devaddr);
	sqlite3_bind_text(stmt, 3, devinfo->appskey_hex, sizeof(devinfo->appskey_hex), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, devinfo->nwkskey_hex, sizeof(devinfo->nwkskey_hex), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 5, devinfo->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 6, devinfo->devaddr);
    DEBUG_STMT(stmt);
    ret = db_step(stmt, NULL, NULL);
    sqlite3_reset(stmt);
    return ret;
}

bool db_insert_upmsg(sqlite3_stmt* stmt, void* devdata, void* metadata, void* payload) {
    bool ret;
    struct devinfo* devinfo = (struct devinfo*) devdata;
    struct metadata* meta = (struct metadata*) metadata;
	sqlite3_bind_int(stmt, 1, meta->tmst);
	sqlite3_bind_text(stmt, 2, meta->datrl, -1, SQLITE_STATIC);
	sqlite3_bind_double(stmt, 3, meta->freq);
	sqlite3_bind_double(stmt, 4, meta->rssi);
	sqlite3_bind_double(stmt, 5, meta->lsnr);
	sqlite3_bind_int(stmt, 6, meta->fcntup);
	sqlite3_bind_text(stmt, 7, meta->gweui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 8, devinfo->appeui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 9, devinfo->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 10, payload, meta->size, SQLITE_STATIC);
    DEBUG_STMT(stmt);
    ret = db_step(stmt, NULL, NULL);
    sqlite3_reset(stmt);
    return ret;
}

bool db_judge_devaddr(sqlite3_stmt* stmt, void* data) {
    bool ret;
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_int(stmt, 1, devinfo->devaddr);
    DEBUG_STMT(stmt);
    ret = db_step(stmt, judge_devaddr, devinfo);
    sqlite3_reset(stmt);
    return ret;
}

bool db_judge_msgrepeat(sqlite3_stmt* stmt, char* deveui, int tmst) {
	sqlite3_bind_text(stmt, 1, deveui, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, tmst);
    DEBUG_STMT(stmt);
	int ret = sqlite3_step(stmt);
    sqlite3_reset(stmt);
	if (ret == SQLITE_ROW) {
        return true; /* a repeatmsg */
    }
    return false;
}

bool db_lookup_nwkskey(sqlite3_stmt* stmt, void* data) {
    bool ret;
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->deveui_hex, sizeof(devinfo->deveui_hex), SQLITE_STATIC);
    DEBUG_STMT(stmt);
    ret = db_step(stmt, lookup_nwkskey, devinfo);
    sqlite3_reset(stmt);
    return ret;
}

bool db_lookup_profile(sqlite3_stmt* stmt, char *gweui, int* rx2dr, float* rx2freq) {
    bool ret;
	sqlite3_bind_text(stmt, 1, gweui, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW) {
	    *rx2dr = sqlite3_column_int(stmt, 0);
	    *rx2freq = sqlite3_column_double(stmt, 1);
	} else {
        *rx2dr = 0; /* default datarate */
        *rx2freq = 869.525; /* default frequence for EU_868 */
    }
    sqlite3_reset(stmt);
    return ret;
}

static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data) {
	int ret;
        //MSG("~INFO~SQL=(%s)\n", sqlite3_expanded_sql(stmt));
	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_DONE) {
			return true;
        } else if (ret == SQLITE_ROW) {
			if (rowcallback != NULL)
				rowcallback(stmt, data);
		} else {
			MSG("sqlite error: %s\n", sqlite3_errstr(ret));
			return false;
		}
	}
}

static void lookup_appkey(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    memset1(devinfo->appkey, 0, sizeof(devinfo->appkey));
    //memcpy1(devinfo->appkey, sqlite3_column_blob(stmt, 0), sizeof(devinfo->appkey));
    str2hex(devinfo->appkey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->appkey));
}


static void lookup_nwkskey(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    memset1(devinfo->nwkskey, 0, sizeof(devinfo->nwkskey));
    str2hex(devinfo->nwkskey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->nwkskey));
}

static void judge_devaddr(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    strncpy((char *)devinfo->deveui_hex, sqlite3_column_text(stmt, 0), sizeof(devinfo->deveui_hex));
}

static void sql_debug(sqlite3_stmt* stmt) { 
    char *sql;
    sql = sqlite3_expanded_sql(stmt);
    MSG_DEBUG(DEBUG_SQL, "\nDBLOG~ SQL=(%s)\n", sql);
    sqlite3_free(sql);
}


