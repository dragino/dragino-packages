/*
 * for sqlite3 databases 
 * db.h 
 *
 *  Created on: Mar 7, 2018
 *      Author: skerlan
 */

#ifndef DB_SQLITE3_H_
#define DB_SQLITE3_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sqlite3.h>


#define MAXCHARSIZE             64
#define SELECTGWEUI             "select gweui from gw where gweui = ?"
#define SELECTJOINNONCE         "select joinnonce from devs where deveui = ?"
#define updatejoinnonce         "update or ignore devs set joinnonce = ? , lastjoin = ?, devaddr = ? where deveui = ?"
#define insertupframe           "insert or ignore into upframes (gweui, deveui, datarate, ulfreq,\
    rssi, snr, fcntup, confirmed, fport, rectime, frmpayload) Values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"


#define INITSTMT(SQL, STMT) if (sqlite3_prepare_v2(db, SQL, -1, &STMT, NULL) != SQLITE_OK) {\
									printf("failed to prepare sql; %s -> %s",\
										SQL, sqlite3_errmsg(db));\
									goto out;\
								}

struct context {
	sqlite3* db;
	// statements for devs
	sqlite3_stmt* lookupstr;
	sqlite3_stmt* insertdevstmt;
	sqlite3_stmt* dev_get_by_eui;
	sqlite3_stmt* dev_get_by_name;
	sqlite3_stmt* listdevsstmt;
	sqlite3_stmt* dev_delete_by_eui;
	//statements for uplinks
	sqlite3_stmt* insertuplink;
	sqlite3_stmt* cleanuplinks;
	sqlite3_stmt* getuplinks_dev;
	// statements for downlinks
	sqlite3_stmt* insertdownlink;
	sqlite3_stmt* cleandownlinks;
	sqlite3_stmt* countdownlinks;
	sqlite3_stmt* downlinks_get_first;
	sqlite3_stmt* downlinks_delete_by_token;
};

int query_db_by_addr(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, uint8_t* data, int size);

int query_db_by_addr_str(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, char* data);

int query_db_by_addr_uint(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, unsigned int* data);

int query_db_by_deveui(sqlite3* db, const char* column_name, const char* table_name, const char* deveui,  uint8_t* data, int size);

int query_db_by_deveui_str(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, char* data);

int query_db_by_deveui_uint(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, unsigned int* data);

int update_db_by_deveui(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, const char* data, int seed);

int update_db_by_deveui_uint(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, unsigned int data);

int update_db_by_addr_uint(sqlite3* db, const char* column_name, const char* table_name,unsigned int devaddr, unsigned int data);

int update_db_by_sqlstr(sqlite3* db, const char* sqlstr);
