/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:
    Network server, sqlite3 db.c

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

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
    sqlite3_stmt* createcfg = NULL;

    sqlite3_stmt* insertgws = NULL;
    sqlite3_stmt* insertapps = NULL;
    sqlite3_stmt* insertdevs = NULL;
    sqlite3_stmt* insertgwprofile = NULL;
    sqlite3_stmt* insertcfg = NULL;

    INITSTMT(CREATEDEVS, createdevs);
    INITSTMT(CREATEGWS, creategws);
    INITSTMT(CREATEAPPS, createapps);
#ifdef LG08_LG02
    INITMSGSTMT(CREATEUPMSG, createupmsg);
#else
    INITSTMT(CREATEUPMSG, createupmsg);
#endif
    INITSTMT(CREATEGWPROFILE, creategwprofile);
    INITSTMT(CREATECFG, createcfg);


    sqlite3_stmt* createstmts[] = { createdevs, creategws, createapps, createupmsg, creategwprofile, createcfg };

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
    INITSTMT(INSERTCFG, insertcfg);

    sqlite3_stmt* insertstmts[] = { insertgws, insertapps, insertdevs, insertgwprofile, insertcfg };

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
                              cntx->judgejoinrepeat, cntx->lookupnwkskey, cntx->lookupprofile,
                              cntx->updategwinfo };

	ret = sqlite3_open(dbpath, &cntx->db);
	if (ret) {
        printf("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->db));
	    sqlite3_close(cntx->db);
		return false;
	}
#ifdef LG08_LG02
	ret = sqlite3_open(MSGDBPATH, &cntx->msgdb);
	if (ret) {
        printf("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->msgdb));
	    sqlite3_close(cntx->msgdb);
		return false;
	}
#endif

    if (!db_createtb(cntx))
        goto out;

    INITSTMT(LOOKUPGWEUI, cntx->lookupgweui);
    INITSTMT(JUDGEJOINREPEAT, cntx->judgejoinrepeat);
    INITSTMT(LOOKUPAPPKEY, cntx->lookupappkey);
    INITSTMT(UPDATEDEVINFO, cntx->updatedevinfo);
#ifdef LG08_LG02
    INITMSGSTMT(INSERTUPMSG, cntx->insertupmsg);
    INITMSGSTMT(JUDGEMSGREPEAT, cntx->judgemsgrepeat);
#else
    INITSTMT(INSERTUPMSG, cntx->insertupmsg);
    INITSTMT(JUDGEMSGREPEAT, cntx->judgemsgrepeat);
#endif
    INITSTMT(JUDGEDEVADDR, cntx->judgedevaddr);
    INITSTMT(LOOKUPNWKSKEY, cntx->lookupnwkskey);
    INITSTMT(LOOKUPPROFILE, cntx->lookupprofile);
    INITSTMT(UPDATEGWINFO, cntx->updategwinfo);

	return true;

out:    
    MSG("GOTO out!\n");
	for (i = 0; i < sizeof(stmts)/sizeof(stmts[0]); i++) {
		sqlite3_finalize(stmts[i]);
	}

	sqlite3_close(cntx->db);
#ifdef LG08_LG02
	sqlite3_close(cntx->msgdb);
#endif
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
		sqlite3_finalize(cntx->updategwinfo);
		sqlite3_close(cntx->db);
}

bool db_lookup_gweui(sqlite3_stmt* stmt, char *gweui) {
	sqlite3_bind_text(stmt, 1, gweui, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
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
    int ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW) {
        memset1(devinfo->appkey, 0, sizeof(devinfo->appkey));
        str2hex(devinfo->appkey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->appkey));
        sqlite3_reset(stmt);
        return true;
    }
    sqlite3_reset(stmt);
    return false;
}

bool db_update_devinfo(sqlite3_stmt* stmt, void* data) {
    bool ret;
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->devnonce_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, devinfo->devaddr_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, devinfo->appskey_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, devinfo->nwkskey_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 5, devinfo->deveui_hex, -1, SQLITE_STATIC);
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
	sqlite3_bind_text(stmt, 9, devinfo->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 10, devinfo->devaddr_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 11, payload, -1, SQLITE_STATIC);
    ret = db_step(stmt, NULL, NULL);
    sqlite3_reset(stmt);
    return ret;
}

bool db_judge_devaddr(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->devaddr_hex, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
    int ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW) {
        strncpy((char *)devinfo->deveui_hex, sqlite3_column_text(stmt, 0), sizeof(devinfo->deveui_hex));
        ret = 0;
    }
    sqlite3_reset(stmt);
    return ret;
}

bool db_judge_msgrepeat(sqlite3_stmt* stmt, char* deveui, uint32_t tmst) {
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
    int ret;
    struct devinfo* devinfo = (struct devinfo*) data;
	sqlite3_bind_text(stmt, 1, devinfo->deveui_hex, -1, SQLITE_STATIC);
    DEBUG_STMT(stmt);
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW) {
        str2hex(devinfo->nwkskey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->nwkskey));
        str2hex(devinfo->appskey, (char *)sqlite3_column_text(stmt, 1), sizeof(devinfo->appskey));
        sqlite3_reset(stmt);
        return true;
    }
    sqlite3_reset(stmt);
    return false;
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

bool db_update_gwinfo(sqlite3_stmt* stmt, void* data) { 
    struct gwinfo* gwinfo = (struct gwinfo*) data;
	sqlite3_bind_text(stmt, 1, gwinfo->time, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, gwinfo->lati);
    sqlite3_bind_double(stmt, 3, gwinfo->longt);
    sqlite3_bind_double(stmt, 4, gwinfo->alti);
    sqlite3_bind_int(stmt, 5, gwinfo->rxnb);
    sqlite3_bind_int(stmt, 6, gwinfo->rxok);
    sqlite3_bind_int(stmt, 7, gwinfo->rxfw);
    sqlite3_bind_int(stmt, 8, gwinfo->ackr);
    sqlite3_bind_int(stmt, 9, gwinfo->dwnb);
    sqlite3_bind_int(stmt, 10, gwinfo->txnb);
	sqlite3_bind_text(stmt, 11, gwinfo->gweui, -1, SQLITE_STATIC);
    int ret = db_step(stmt, NULL, NULL);
    sqlite3_reset(stmt);
    return ret;
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
			MSG_DEBUG(DEBUG_ERROR, "ERROR~ %s\n", sqlite3_errstr(ret));
			return false;
		}
	}
}

static void lookup_appkey(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    memset1(devinfo->appkey, 0, sizeof(devinfo->appkey));
    str2hex(devinfo->appkey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->appkey));
}

static void lookup_nwkskey(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    memset1(devinfo->nwkskey, 0, sizeof(devinfo->nwkskey));
    str2hex(devinfo->nwkskey, (char *)sqlite3_column_text(stmt, 0), sizeof(devinfo->nwkskey));
    MSG_DEBUG(DEBUG_DEBUG, "\nNWKskey: %02X%02X\n", devinfo->nwkskey[3], devinfo->nwkskey[4]);
}

static void judge_devaddr(sqlite3_stmt* stmt, void* data) {
    struct devinfo* devinfo = (struct devinfo*) data;
    strncpy((char *)devinfo->deveui_hex, sqlite3_column_text(stmt, 0), sizeof(devinfo->deveui_hex));
}

static void sql_debug(sqlite3_stmt* stmt) { 
    char *sql;
    sql = sqlite3_expanded_sql(stmt);
    MSG_DEBUG(DEBUG_SQL, "\nDEBUG-DB~ SQL=(%s)\n", sql);
    sqlite3_free(sql);
}

