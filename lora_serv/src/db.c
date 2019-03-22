/*
 * for sqlite3 databases 
 * db.c
 *
 *  Created on: Mar 7, 2018
 *      Author: skerlan
 */

#include <string.h>
#include "db.h"

//char array to hex array
static void str_to_hex(uint8_t* dest, char* src, int len);

static void lookup_gweui(sqlite3_stmt* stmt, void* data); 
static void judge_joininfo(sqlite3_stmt* stmt, void* data);
static void judge_devaddr(sqlite3_stmt* stmt, void* data);

bool db_init(const char* dbpath, struct context* cntx) {
	sqlite3_stmt* stmts[] = { cntx->selectgweui,
                                  cntx->selectjoinnonce,
                                }

	int ret = sqlite3_open(dbpath, &cntx->db);
	if (ret) {
                printf("ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(cntx->db);
		goto out;
	}

        INITSTMT(SELECTGWEUI, cntx->selectgweui);

	return SUCCESS;

out:    
        int i;
	for (i = 0; i < sizeof(stmts)/sizeof(stmts[0]); i++) {
		sqlite3_finalize(stmts[i]);
	}
        return FAILED;

}

static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data) {
	int ret;
	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_DONE)
			return TRUE;
		else if (ret == SQLITE_ROW) {
			if (rowcallback != NULL)
				rowcallback(stmt, data);
		} else {
			printf("sqlite error: %s", sqlite3_errstr(ret));
			return false;
		}
	}
}

bool db_lookup_gweui(sqlite3_stmt* stmt, struct metadata* meta) {
	sqlite3_bind_text(stmt, 1, meta->gweui_hex, -1, SQLITE_STATIC);
        db_step(stmt, lookup_str, meta);
        sqlite3_reset(stmt);
        if (!strcmp(meta->gweui, meta->outstr))  
                return true; /* gweui exist */
        else
                return false;
}

bool db_judge_joinrepeat(sqlite3_stmt* stmt, struct metadata* meta) {
	sqlite3_bind_text(stmt, 1, meta->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, meta->devnonce, sizeof(meta->devnonce), SQLITE_STATIC);
        db_step(stmt, lookup_str, meta);
        sqlite3_reset(stmt);
        if (!strcmp(meta->appeui_hex, meta->outstr))  
                return true; /* devnonce exist */
        else
                return false;
e

bool db_lookup_appkey(sqlite3_stmt* stmt, struct metadata* meta) {
	sqlite3_bind_text(stmt, 1, meta->appeui_hex, -1, SQLITE_STATIC);
        db_step(stmt, lookup_appkey, meta);
        sqlite3_reset(stmt);
}

bool db_update_devinfo(sqlite3_stmt* stmt, struct metadata* meta) {
        bool ret;
	sqlite3_bind_blob(stmt, 1, meta->devnonce, sizeof(meta->devnonce), SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, meta->devaddr, sizeof(meta->devaddr), SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 3, meta->appskey, sizeof(meta->appskey), SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 4, meta->nwkskey, sizeof(meta->nwkskey), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 5, meta->deveui_hex, -1, SQLITE_STATIC);
        ret = db_step(stmt, NULL, NULL);
        sqlite3_reset(stmt);
        return ret;
}

bool db_insert_upmsg(sqlite3_stmt* stmt, struct metadata* meta, int fcntup, void *payload, psize) {
        bool ret;
	sqlite3_bind_int(stmt, 1, meta->tmst);
	sqlite3_bind_int(stmt, 2, meta->datarate);
	sqlite3_bind_double(stmt, 3, meta->freq);
	sqlite3_bind_double(stmt, 4, meta->rssi);
	sqlite3_bind_double(stmt, 5, meta->snr);
	sqlite3_bind_int(stmt, 6, fcntup);
	sqlite3_bind_text(stmt, 7, meta->gweui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 8, meta->appeui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 9, meta->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 10, payload, psize, SQLITE_STATIC);
        ret = db_step(stmt, NULL, NULL);
        sqlite3_reset(stmt);
        return ret;
}

bool db_judge_devaddr(sqlite3_stmt* stmt, struct metadata* meta) {
        bool ret;
	sqlite3_bind_blob(stmt, 1, meta->devaddr, sizeof(meta->devaddr), SQLITE_STATIC);
        ret = db_step(stmt, judge_devaddr, meta);
        sqlite3_reset(stmt);
        return ret;
}

bool db_judge_msgrepeat(sqlite3_stmt* stmt, struct metadata* meta) {
	sqlite3_bind_text(stmt, 1, meta->deveui_hex, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, meta->tmst);
        db_step(stmt, lookupstr, meta);
        sqlite3_reset(stmt);
        if (!strcmp(meta->deveui_hex, meta->outstr)) 
                return true; /* a repeatmsg */
        return false;
}

bool db_lookup_nwkskey(sqlite3_stmt* stmt, struct metadata* meta) {
        bool ret;
	sqlite3_bind_text(stmt, 1, meta->deveui_hex, sizeof(meta->deveui_hex), SQLITE_STATIC);
        ret = db_step(stmt, lookup_nwkskey, meta);
        sqlite3_reset(stmt);
        return ret;
}

static void lookup_str(sqlite3_stmt* stmt, void* data) {
        struct metadata* meta = (struct metadata*) data;
        memset(meta->outstr, 0, sizeof(meta->outstr));
        strncpy((char *)meta->outstr, sqlite3_column_text(stmt, 0), sizeof(meta->outstr));
}

static void lookup_appkey(sqlite3_stmt* stmt, void* data) {
        struct metadata* meta = (struct metadata*) data;
        memset1(meta->appkey, 0, sizeof(meta->appkey));
        memcpy1(meta->appkey, sqlite3_column_blob(stmt, 0), sizeof(meta->appkey));
}

static void lookup_nwkskey(sqlite3_stmt* stmt, void* data) {
        struct metadata* meta = (struct metadata*) data;
        memset1(meta->nwkskey, 0, sizeof(meta->nwkskey));
        memcpy1(meta->nwkskey, sqlite3_column_blob(stmt, 0), sizeof(meta->nwkskey));
}

static void dbgetmetainfo(sqlite3_stmt* stmt, void* data) {
        struct metadata* meta = (struct metadata*) data;

        memset1(meta->devnonce, 0, sizeof(meta->devnonce));
        memset(meta->appeui_hex, 0, sizeof(meta->appeui_hex));
        memset1(meta->appskey, 0, sizeof(meta->appskey));
        memset1(meta->nwkskey, 0, sizeof(meta->nwkskey));

        memcpy1(meta->devnonce, sqlite3_column_blob(stmt, 0), sizeof(meta->devnonce));
        strncpy(meta->appeui_hex, sqlite3_column_text(stmt, 1), sizeof(meta->appeui_hex));
        memcpy1(meta->nwkskey, sqlite3_column_blob(stmt, 2), sizeof(meta->nwkskey));
        memcpy1(meta->appskey, sqlite3_column_blob(stmt, 3), sizeof(meta->appskey));
}

static void judge_devaddr(sqlite3_stmt* stmt, void* data) {
        struct metadata* meta = (struct metadata*) data;
        memset(meta->deveui_hex, 0, sizeof(meta->deveui_hex));
        strncpy((char *)meta->deveui_hex, sqlite3_column_text(stmt, 0), sizeof(meta->deveui_hex));
}

void str_to_hex(uint8_t* dest, char* src, int len) {
	int i = 0;
	char ch1;
	char ch2;
	uint8_t ui1;
	uint8_t ui2;

	for (i = 0; i < len; i++) {
		ch1 = src[i*2];
		ch2 = src[i*2 + 1];
		ui1 = toupper(ch1) - 0x30;
		if (ui1 > 9)
			ui1 -= 7;
		ui2 = toupper(ch2) - 0x30;
		if (ui2 > 9)
			ui2 -= 7;
		dest[i] = ui1 * 16 + ui2;
	}
}



