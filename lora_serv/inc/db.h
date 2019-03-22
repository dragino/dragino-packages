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
#include <stdbool.h>
#include <sqlite3.h>

#define LOOKUPGWEUI "select gweui from gws where gweui = ?"
#define JUDGEJOINREPEAT "select appeui devs where deveui = ? and devnonce = ?"
#define LOOKUPAPPKEY "select appkey from apps where appeui = ?"
#define UPDATEDEVINFO "update or ignore devs set devnonce = ?, devaddr = ?, appskey = ?, nwkskey = ? where deveui = ?"
#define INSERTUPMSG "insert or ignore into upframes (tmst, datarate, freq, rssi, snr, fcntup, gweui, appeui, deveui, frmpayload) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define JUDGEDEVADDR "select deveui from devs where devaddr = ?"
#define JUDGEMSGREPEAT "select deveui from upmsg where deveui = ? and tmst = ?"
#define LOOKUPNWKSKEY "select nwkskey from devs where deveui = ?"

#define INITSTMT(SQL, STMT) if (sqlite3_prepare_v2(db, SQL, -1, &STMT, NULL) != SQLITE_OK) {\
									printf("failed to prepare sql; %s -> %s",\
										SQL, sqlite3_errmsg(db));\
									goto out;\
								}

struct context {
	sqlite3* db;
	// statements for devs
	sqlite3_stmt* lookupstr;
};

