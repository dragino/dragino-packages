/*
 * for sqlite3 databases 
 * db.c
 *
 *  Created on: Mar 7, 2018
 *      Author: skerlan
 */

#include <string.h>
#include "db.h"

uint8_t* result;
char sql[128];

//char array to hex array
static void str_to_hex(uint8_t* dest, char* src, int len);

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
			printf("sqlite error; %s", sqlite3_errstr(ret));
			return false;
		}
	}
}

bool db_lookup_str(sqlite3_stmt* smtm, char* value, char* output) {

	sqlite3_bind_text(smtm, 1, value, -1, SQLITE_STATIC);
        db_step(smtm, query_db, output);
        sqlite3_reset(smtm);
        if (!strcmp(value, output))
                return true;
        else
                return false;
}

bool db_lookup_int(sqlite3_stmt* stmt, int value, int* output) {

	sqlite3_bind_text(smtm, 1, value, -1, SQLITE_STATIC);
        db_step(smtm, query_db, output);
        sqlite3_reset(smtm);
        if (value == *output)
                return true;
        else
                return false;
}
                
void query_db(sqlite3_stmt* stmt, void* data) {
        switch (sqlite3_column_type(stmt, 0)) {
                case 1:
                        (int)*data = 0;
                        (int)*data = sqlite3_column_int(stmt, 0);
                        break;
                default:
                        bzero((char 8)data, MAXSTRSIZE);
                        strncpy((char *)data, sqlite3_column_text(stmt, 0), MAXCHARSIZE);
                        return;
        }
}
                        
bool update_db_by_sqlstr(sqlite3* db, const char* sqlstr) {

        if (sqlite3_exec(db, sqlstr, NULL, NULL, NULL) != SQLITE_OK) 
            return FAILED;
        else
            return SUCCESS;
}

/*try to query one field from one table by devaddr filed
 *there is only one record in query results
 */

int query_db_by_addr(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, uint8_t* data, int size) {

	int ret = 0;

        char tempchar[64] = {'\0'};

        sqlite3_stmt *ppstmt; 

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "SELECT HEX(%s) AS %s FROM %s WHERE devaddr=%u", column_name, column_name, table_name, devaddr);

        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                strncpy(tempchar, (uint8_t *)sqlite3_column_text(ppstmt, 0), sizeof(tempchar));
                ret = 1;
        }

        sqlite3_finalize(ppstmt);

	if(strlen(tempchar) != size * 2){
		return FAILED;
	}

	str_to_hex(data, tempchar, size);

        return ret;
}

int query_db_by_addr_str(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, char* data) {

	int ret = 0;

        sqlite3_stmt *ppstmt; 

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "SELECT HEX(%s) AS %s FROM %s WHERE devaddr=%u", column_name, column_name, table_name, devaddr);
        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                strcpy(data, (uint8_t *)sqlite3_column_text(ppstmt, 0));
                ret = 1;
        }
        sqlite3_finalize(ppstmt);
        return ret;
}

int query_db_by_addr_uint(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, unsigned int* data) {

	int ret = 0;

        sqlite3_stmt *ppstmt; 

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "SELECT %s FROM %s WHERE devaddr=%u", column_name, table_name, devaddr);
        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                *data = sqlite3_column_int(ppstmt, 0);
                ret = 1;
        }
        sqlite3_finalize(ppstmt);
        return ret;
}

/*try to query one field from one table by devaddr filed
 *there is only one record in query results
 */
int query_db_by_deveui(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, uint8_t* data, int size){

	int ret = 0;

        sqlite3_stmt *ppstmt; 

        char tempchar[64];

	bzero(sql, sizeof(sql));

	snprintf(sql, sizeof(sql), "SELECT HEX(%s) AS %s FROM %s WHERE deveui=x'%s'", column_name, column_name, table_name, deveui);
	
        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                strncpy(tempchar, sqlite3_column_text(ppstmt, 0), sizeof(tempchar));
                ret = 1;
        }
        sqlite3_finalize(ppstmt);

	if(strlen(tempchar) != size * 2){
		return FAILED;
	}
	//char array to hex array
	str_to_hex(data, tempchar, size);

	return SUCCESS;
}

int query_db_by_deveui_str(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, char* data){

	int ret = 0;

        sqlite3_stmt *ppstmt; 

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "SELECT %s FROM %s WHERE deveui=x'%s'", column_name, table_name, deveui);
        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                strcpy(data, (uint8_t *)sqlite3_column_text(ppstmt, 0));
                ret = 1;
        }
        sqlite3_finalize(ppstmt);
	return ret;
}

int query_db_by_deveui_uint(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, unsigned int* data) {

	int ret = 0;

        sqlite3_stmt *ppstmt; 

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "SELECT %s FROM %s WHERE deveui=x'%s'", column_name, table_name, deveui);

        if (sqlite3_prepare_v2(db, sql, strlen(sql), &ppstmt, NULL) != SQLITE_OK)
                return FAILED;

        if (sqlite3_step(ppstmt) == SQLITE_ROW) {
                *data = sqlite3_column_int(ppstmt, 0);
                ret = 1;
        }
        sqlite3_finalize(ppstmt);
        return ret;
}

int update_db_by_deveui(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, const char* data, int seed) {

	bzero(sql, sizeof(sql));

	if (seed == 0) {
		snprintf(sql, sizeof(sql), "UPDATE %s SET %s=x'%s' WHERE deveui=x'%s'", table_name, column_name, data, deveui);
	} else {
		snprintf(sql, sizeof(sql), "UPDATE %s SET %s=\"%s\" WHERE deveui=x'%s'", table_name, column_name, data, deveui);
	}

        if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) 
            return FAILED;
        else
            return SUCCESS;
}

int update_db_by_deveui_uint(sqlite3* db, const char* column_name, const char* table_name, const char* deveui, unsigned int data) {

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql), "UPDATE %s SET %s=%u WHERE deveui=x'%s'", table_name, column_name, data, deveui);
        if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) 
            return FAILED;
        else
            return SUCCESS;
}

int update_db_by_addr_uint(sqlite3* db, const char* column_name, const char* table_name, unsigned int devaddr, unsigned int data) {

	bzero(sql, sizeof(sql));
	snprintf(sql, sizeof(sql),"UPDATE %s SET %s=%u WHERE devaddr=%u", table_name, column_name, data, devaddr);
        if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) 
            return FAILED;
        else
            return SUCCESS;
}


void str_to_hex(uint8_t* dest, char* src, int len) {
	int i = 0;
	char ch1;
	char ch2;
	uint8_t ui1;
	uint8_t ui2;

	for (i=0; i<len; i++) {
		ch1 = src[i*2];
		ch2 = src[i*2+1];
		ui1 = toupper(ch1) - 0x30;
		if (ui1 > 9)
			ui1 -= 7;
		ui2 = toupper(ch2) - 0x30;
		if (ui2 > 9)
			ui2 -= 7;
		dest[i] = ui1 * 16 + ui2;
	}
}



