#include <sqlite3.h>

#include "database.h"
#include "utils.h"

#include "control.sqlite.h"
#include "join.sqlite.h"
#include "uplink.sqlite.h"
#include "downlink.sqlite.h"

#include "config.h"

//sql for apps
#define LIST_APPFLAGS	"SELECT flag FROM appflags WHERE eui = ?;"

//sql for devs
#define LIST_DEVFLAGS	"SELECT flag FROM devflags WHERE eui = ?;"

//sql for sessions
#define GET_SESSIONDEVADDR	"SELECT * FROM sessions WHERE devaddr = ?;"
#define GET_KEYPARTS		"SELECT key,appnonce,devnonce,appeui,deveui "\
								"FROM sessions INNER JOIN devs on devs.eui = sessions.deveui WHERE devaddr = ?"
#define GET_FRAMECOUNTER_UP			"SELECT upcounter FROM sessions WHERE devaddr = ?"
#define GET_FRAMECOUNTER_DOWN		"SELECT downcounter FROM sessions WHERE devaddr = ?"
#define SET_FRAMECOUNTER_UP		"UPDATE sessions SET upcounter = ? WHERE devaddr = ?"
#define INC_FRAMECOUNTER_DOWN		"UPDATE sessions SET downcounter = downcounter +  1 WHERE devaddr = ?"

//sql for uplinks
#define CLEAN_UPLINKS	"DELETE FROM uplinks WHERE (timestamp < ?);"

//sql for downlinks
#define CLEAN_DOWNLINKS "DELETE FROM downlinks WHERE ((? - timestamp)/1000000) > deadline"
#define COUNT_DOWNLINKS	"SELECT count(*) FROM downlinks WHERE appeui = ? AND deveui = ?"
#define DOWNLINKS_GET_FIRST "SELECT timestamp,deadline,appeui,deveui,port,payload,confirm,token FROM `downlinks` ORDER BY `timestamp` DESC LIMIT 1"

#define ISSQLITEERROR(v) ((v & SQLITE_ERROR) == SQLITE_ERROR)

static gboolean database_stepuntilcomplete(sqlite3_stmt* stmt,
		void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data) {
	int ret;
	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_DONE)
			return TRUE;
		else if (ret == SQLITE_ROW) {
			if (rowcallback != NULL)
				rowcallback(stmt, data);
		} else if (ISSQLITEERROR(ret)) {
			g_message("sqlite error; %s", sqlite3_errstr(ret));
			return false;
		}
	}
}

#define INITSTMT(SQL, STMT) if (sqlite3_prepare_v2(cntx->dbcntx.db, SQL,\
								-1, &STMT, NULL) != SQLITE_OK) {\
									g_message("failed to prepare sql; %s -> %s",\
                                                                                        SQL, sqlite3_errmsg(cntx->dbcntx.db));\
									goto out;\
								}

#define LOOKUPBYSTRING(stmt, string, convertor)\
		const struct pair callbackanddata = { .first = callback, .second = data };\
			sqlite3_bind_text(stmt, 1, string, -1, SQLITE_STATIC);\
			database_stepuntilcomplete(stmt, convertor,\
					(void*) &callbackanddata);\
			sqlite3_reset(stmt)

static gboolean database_init_createtables(struct context* cntx) {
	sqlite3_stmt* createappsstmt = NULL;
	sqlite3_stmt* createappflagsstmt = NULL;
	sqlite3_stmt* createdevsstmt = NULL;
	sqlite3_stmt* createdevflagsstmt = NULL;
	sqlite3_stmt* createsessionsstmt = NULL;
	sqlite3_stmt* createuplinksstmt = NULL;
	sqlite3_stmt* createdownlinksstmt = NULL;

	INITSTMT(__SQLITEGEN_APPS_TABLE_CREATE, createappsstmt);
	INITSTMT(__SQLITEGEN_APPFLAGS_TABLE_CREATE, createappflagsstmt);
	INITSTMT(__SQLITEGEN_DEVS_TABLE_CREATE, createdevsstmt);
	INITSTMT(__SQLITEGEN_DEVFLAGS_TABLE_CREATE, createdevflagsstmt);
	INITSTMT(__SQLITEGEN_SESSIONS_TABLE_CREATE, createsessionsstmt);
	INITSTMT(__SQLITEGEN_UPLINKS_TABLE_CREATE, createuplinksstmt);
	INITSTMT(__SQLITEGEN_DOWNLINKS_TABLE_CREATE, createdownlinksstmt);

	sqlite3_stmt* createstmts[] = { createappsstmt, createappflagsstmt,
			createdevsstmt, createdevflagsstmt, createsessionsstmt,
			createuplinksstmt, createdownlinksstmt };

	for (int i = 0; i < G_N_ELEMENTS(createstmts); i++) {
		if (!database_stepuntilcomplete(createstmts[i], NULL, NULL)) {
			g_message("error running table create statement %d", i);
			goto out;
		}
		sqlite3_finalize(createstmts[i]);
	}

	return TRUE;

	out: return FALSE;
}

gboolean database_init(struct context* cntx, const gchar* databasepath) {
#if TWLBE_DEBUG
	g_message("database is in %s", databasepath);
#endif

	sqlite3_stmt* stmts[] = { cntx->dbcntx.insertappstmt,
			cntx->dbcntx.app_get_by_eui, cntx->dbcntx.app_get_by_name,
			cntx->dbcntx.listappsstmt, cntx->dbcntx.app_delete_by_eui,
			cntx->dbcntx.listappflagsstmt, cntx->dbcntx.insertdevstmt,
			cntx->dbcntx.dev_get_by_eui, cntx->dbcntx.dev_get_by_name,
			cntx->dbcntx.dev_delete_by_eui, cntx->dbcntx.listdevsstmt,
			cntx->dbcntx.insertsessionstmt, cntx->dbcntx.getsessionbydeveuistmt,
			cntx->dbcntx.getsessionbydevaddrstmt,
			cntx->dbcntx.deletesessionstmt, cntx->dbcntx.getkeyparts,
			cntx->dbcntx.getframecounterup, cntx->dbcntx.getframecounterdown,
			cntx->dbcntx.setframecounterup, cntx->dbcntx.incframecounterdown,
			cntx->dbcntx.insertuplink, cntx->dbcntx.getuplinks_dev,
			cntx->dbcntx.cleanuplinks, cntx->dbcntx.insertdownlink,
			cntx->dbcntx.cleandownlinks, cntx->dbcntx.countdownlinks,
			cntx->dbcntx.downlinks_get_first };

	if (g_mkdir_with_parents(TLWBE_STATEDIR, 0660) != 0) {
		g_message("failed to create state directory");
		goto out;
	}

	int ret = sqlite3_open(databasepath, &cntx->dbcntx.db);
	if (ret) {
		g_message("failed to open database %s", databasepath);
		sqlite3_close(cntx->dbcntx.db);
		goto out;
	}

	if (!database_init_createtables(cntx))
		goto out;

	INITSTMT(__SQLITEGEN_APPS_INSERT, cntx->dbcntx.insertappstmt);
	INITSTMT(__SQLITEGEN_APPS_GETBY_EUI, cntx->dbcntx.app_get_by_eui);
	INITSTMT(__SQLITEGEN_APPS_GETBY_NAME, cntx->dbcntx.app_get_by_name);
	INITSTMT(__SQLITEGEN_APPS_LIST_EUI, cntx->dbcntx.listappsstmt);
	INITSTMT(__SQLITEGEN_APPS_DELETEBY_EUI, cntx->dbcntx.app_delete_by_eui);
	INITSTMT(LIST_APPFLAGS, cntx->dbcntx.listappflagsstmt);

	INITSTMT(__SQLITEGEN_DEVS_INSERT, cntx->dbcntx.insertdevstmt);
	INITSTMT(__SQLITEGEN_DEVS_GETBY_EUI, cntx->dbcntx.dev_get_by_eui);
	INITSTMT(__SQLITEGEN_DEVS_GETBY_NAME, cntx->dbcntx.dev_get_by_name);
	INITSTMT(__SQLITEGEN_DEVS_DELETEBY_EUI, cntx->dbcntx.dev_delete_by_eui);
	INITSTMT(__SQLITEGEN_DEVS_LIST_EUI, cntx->dbcntx.listdevsstmt);

	INITSTMT(__SQLITEGEN_SESSIONS_INSERT, cntx->dbcntx.insertsessionstmt);
	INITSTMT(__SQLITEGEN_SESSIONS_GETBY_DEVEUI,
			cntx->dbcntx.getsessionbydeveuistmt);
	INITSTMT(GET_SESSIONDEVADDR, cntx->dbcntx.getsessionbydevaddrstmt);
	INITSTMT(__SQLITEGEN_SESSIONS_DELETEBY_DEVEUI,
			cntx->dbcntx.deletesessionstmt);

	INITSTMT(GET_KEYPARTS, cntx->dbcntx.getkeyparts);

	INITSTMT(GET_FRAMECOUNTER_UP, cntx->dbcntx.getframecounterup);
	INITSTMT(GET_FRAMECOUNTER_DOWN, cntx->dbcntx.getframecounterdown);
	INITSTMT(SET_FRAMECOUNTER_UP, cntx->dbcntx.setframecounterup);
	INITSTMT(INC_FRAMECOUNTER_DOWN, cntx->dbcntx.incframecounterdown);

	INITSTMT(__SQLITEGEN_UPLINKS_INSERT, cntx->dbcntx.insertuplink);
	INITSTMT(__SQLITEGEN_UPLINKS_GETBY_DEVEUI, cntx->dbcntx.getuplinks_dev);
	INITSTMT(CLEAN_UPLINKS, cntx->dbcntx.cleanuplinks);

	INITSTMT(__SQLITEGEN_DOWNLINKS_INSERT, cntx->dbcntx.insertdownlink);
	INITSTMT(CLEAN_DOWNLINKS, cntx->dbcntx.cleandownlinks);
	INITSTMT(COUNT_DOWNLINKS, cntx->dbcntx.countdownlinks);
	INITSTMT(DOWNLINKS_GET_FIRST, cntx->dbcntx.downlinks_get_first);
	INITSTMT(__SQLITEGEN_DOWNLINKS_DELETEBY_TOKEN,
			cntx->dbcntx.downlinks_delete_by_token);

	return TRUE;

	out:

	for (int i = 0; i < G_N_ELEMENTS(stmts); i++) {
		sqlite3_finalize(stmts[i]);
	}

	return FALSE;
}

void database_app_add(context_readonly* cntx, const struct app* app) {
	__sqlitegen_apps_add(cntx->dbcntx.insertappstmt, app);
	database_stepuntilcomplete(cntx->dbcntx.insertappstmt, NULL, NULL);
	sqlite3_reset(cntx->dbcntx.insertappstmt);
}

void database_app_update(context_readonly* cntx, const struct app* app) {

}

static void database_app_get_rowcallback(sqlite3_stmt* stmt, void* data) {
	struct pair* callbackanddata = data;

	const gchar* eui = (const gchar*) sqlite3_column_text(stmt, 0);
	const gchar* name = (const gchar*) sqlite3_column_text(stmt, 1);
	int serial = sqlite3_column_int(stmt, 2);

	const struct app a = { .eui = eui, .name = name, .serial = serial };

	((void (*)(const struct app*, void*)) callbackanddata->first)(&a,
			callbackanddata->second);
}

void database_app_get(context_readonly* cntx, const char* eui, const char* name,
		void (*callback)(const struct app* app, void* data), void* data) {
	g_assert(eui != NULL || name != NULL);
	if (eui != NULL) {
		LOOKUPBYSTRING(cntx->dbcntx.app_get_by_eui, eui,
				database_app_get_rowcallback);
	} else if (name != NULL) {
		LOOKUPBYSTRING(cntx->dbcntx.app_get_by_name, name,
				database_app_get_rowcallback);
	}
}

void database_app_del(context_readonly* cntx, const char* eui) {
	sqlite3_bind_text(cntx->dbcntx.app_delete_by_eui, 1, eui, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.app_delete_by_eui,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.app_delete_by_eui);
}

static void database_apps_list_rowcallback(sqlite3_stmt* stmt, void* data) {
	const struct pair* callbackanddata = data;
	const unsigned char* eui = sqlite3_column_text(stmt, 0);
	((void (*)(const char*, void*)) callbackanddata->first)(eui,
			callbackanddata->second);
}

void database_apps_list(context_readonly* cntx,
		void (*callback)(const char* eui, void* data), void* data) {
	const struct pair callbackanddata = { .first = callback, .second = data };
	sqlite3_stmt* stmt = cntx->dbcntx.listappsstmt;
	database_stepuntilcomplete(stmt, database_apps_list_rowcallback,
			(void*) &callbackanddata);
	sqlite3_reset(stmt);
}

static void data_appflags_list_rowcallback(sqlite3_stmt* stmt, void* data) {

}

void database_appflags_list(context_readonly* cntx, const char* appeui,
		void (*callback)(const char* flag, void* data), void* data) {
	LOOKUPBYSTRING(cntx->dbcntx.listappflagsstmt, appeui,
			data_appflags_list_rowcallback);
}

void database_dev_add(context_readonly* cntx, const struct dev* dev) {
	sqlite3_stmt* stmt = cntx->dbcntx.insertdevstmt;
	__sqlitegen_devs_add(stmt, dev);
	database_stepuntilcomplete(stmt, NULL, NULL);
	sqlite3_reset(stmt);
}

void database_dev_update(context_readonly* cntx, const struct dev* dev) {

}

static void database_dev_get_rowcallback(sqlite3_stmt* stmt, void* data) {
	struct pair* callbackanddata = data;

	const gchar* eui = (const gchar*) sqlite3_column_text(stmt, 0);
	const gchar* appeui = (const gchar*) sqlite3_column_text(stmt, 1);
	const gchar* key = (const gchar*) sqlite3_column_text(stmt, 2);
	const gchar* name = (const gchar*) sqlite3_column_text(stmt, 3);
	int serial = sqlite3_column_int(stmt, 4);

	const struct dev d = { .eui = eui, .appeui = appeui, .key = key, .name =
			name, .serial = serial };

	((void (*)(const struct dev*, void*)) callbackanddata->first)(&d,
			callbackanddata->second);
}

void database_dev_get(context_readonly* cntx, const char* eui, const char* name,
		void (*callback)(const struct dev* app, void* data), void* data) {
	g_assert(eui != NULL || name != NULL);
	if (eui != NULL) {
		LOOKUPBYSTRING(cntx->dbcntx.dev_get_by_eui, eui,
				database_dev_get_rowcallback);
	} else if (name != NULL) {
		LOOKUPBYSTRING(cntx->dbcntx.dev_get_by_name, name,
				database_dev_get_rowcallback);
	}
}

void database_dev_del(context_readonly* cntx, const char* eui) {
	sqlite3_bind_text(cntx->dbcntx.dev_delete_by_eui, 1, eui, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.dev_delete_by_eui,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.dev_delete_by_eui);
}

void database_devs_list(context_readonly* cntx,
		void (*callback)(const char* eui, void* data), void* data) {
	const struct pair callbackanddata = { .first = callback, .second = data };
	sqlite3_stmt* stmt = cntx->dbcntx.listdevsstmt;
	database_stepuntilcomplete(stmt, database_apps_list_rowcallback,
			(void*) &callbackanddata);
	sqlite3_reset(stmt);
}

gboolean database_session_add(context_readonly* cntx,
		const struct session* session) {
	sqlite3_stmt* stmt = cntx->dbcntx.insertsessionstmt;
	__sqlitegen_sessions_add(stmt, session);
	database_stepuntilcomplete(stmt, NULL, NULL);
	sqlite3_reset(stmt);
	return TRUE;
}

static void database_session_get_rowcallback(sqlite3_stmt* stmt, void* data) {
	struct pair* callbackanddata = data;

	const gchar* deveui = (const gchar*) sqlite3_column_text(stmt, 0);
	const gchar* devnonce = (const gchar*) sqlite3_column_text(stmt, 1);
	const gchar* appnonce = (const gchar*) sqlite3_column_text(stmt, 2);
	const gchar* devaddr = (const gchar*) sqlite3_column_text(stmt, 3);

	const struct session s = { .deveui = deveui, .devnonce = devnonce,
			.appnonce = appnonce, .devaddr = devaddr };

	((void (*)(const struct session*, void*)) callbackanddata->first)(&s,
			callbackanddata->second);
}

void database_session_get_deveui(context_readonly* cntx, const char* deveui,
		void (*callback)(const struct session* session, void* data), void* data) {
	const struct pair callbackanddata = { .first = callback, .second = data };
	sqlite3_stmt* stmt = cntx->dbcntx.getsessionbydeveuistmt;
	sqlite3_bind_text(stmt, 1, deveui, -1, SQLITE_STATIC);
	database_stepuntilcomplete(stmt, database_session_get_rowcallback,
			(void*) &callbackanddata);
	sqlite3_reset(stmt);
}

void database_session_get_devaddr(context_readonly* cntx, const char* devaddr,
		void (*callback)(const struct session* session, void* data), void* data) {
	const struct pair callbackanddata = { .first = callback, .second = data };
	sqlite3_stmt* stmt = cntx->dbcntx.getsessionbydevaddrstmt;
	sqlite3_bind_text(stmt, 1, devaddr, -1, SQLITE_STATIC);
	database_stepuntilcomplete(stmt, database_session_get_rowcallback,
			(void*) &callbackanddata);
	sqlite3_reset(stmt);
}

void database_session_del(context_readonly* cntx, const char* deveui) {
	sqlite3_stmt* stmt = cntx->dbcntx.deletesessionstmt;
	sqlite3_bind_text(stmt, 1, deveui, -1, SQLITE_STATIC);
	database_stepuntilcomplete(stmt, NULL, NULL);
	sqlite3_reset(stmt);
}

static void database_keyparts_get_rowcallback(sqlite3_stmt* stmt, void* data) {
	struct pair* callbackanddata = data;

	const gchar* key = (const gchar*) sqlite3_column_text(stmt, 0);
	const gchar* appnonce = (const gchar*) sqlite3_column_text(stmt, 1);
	const gchar* devnonce = (const gchar*) sqlite3_column_text(stmt, 2);
	const gchar* appeui = (const gchar*) sqlite3_column_text(stmt, 3);
	const gchar* deveui = (const gchar*) sqlite3_column_text(stmt, 4);

	const struct keyparts kp = { .key = key, .appnonce = appnonce, .devnonce =
			devnonce, .appeui = appeui, .deveui = deveui };

	((void (*)(const struct keyparts*, void*)) callbackanddata->first)(&kp,
			callbackanddata->second);
}

void database_keyparts_get(context_readonly* cntx, const char* devaddr,
		void (*callback)(const struct keyparts* keyparts, void* data),
		void* data) {
	LOOKUPBYSTRING(cntx->dbcntx.getkeyparts, devaddr,
			database_keyparts_get_rowcallback);
}

static void database_framecounter_get_rowcallback(sqlite3_stmt* stmt,
		void* data) {
	int* framecounter = data;
	*framecounter = sqlite3_column_int(stmt, 0);
}

int database_framecounter_down_getandinc(context_readonly* cntx,
		const char* devaddr) {
	int framecounter = -1;
	sqlite3_bind_text(cntx->dbcntx.getframecounterdown, 1, devaddr, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.getframecounterdown,
			database_framecounter_get_rowcallback, &framecounter);
	sqlite3_reset(cntx->dbcntx.getframecounterdown);
	sqlite3_bind_text(cntx->dbcntx.incframecounterdown, 1, devaddr, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.incframecounterdown,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.incframecounterdown);
	return framecounter;
}

void database_framecounter_up_set(context_readonly* cntx, const char* devaddr,
		int framecounter) {
	sqlite3_bind_int(cntx->dbcntx.setframecounterup, 1, framecounter);
	sqlite3_bind_text(cntx->dbcntx.setframecounterup, 2, devaddr, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.setframecounterup,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.setframecounterup);
}

void database_uplink_add(context_readonly* cntx, struct uplink* uplink) {
	__sqlitegen_uplinks_add(cntx->dbcntx.insertuplink, uplink);
	database_stepuntilcomplete(cntx->dbcntx.insertuplink,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.insertuplink);
}

static void database_uplinks_get_rowcallback(sqlite3_stmt* stmt, void* data) {
	struct pair* callbackanddata = data;

	guint64 timestamp = sqlite3_column_int64(stmt, 1);
	const gchar* appeui = (const gchar*) sqlite3_column_text(stmt, 2);
	const gchar* deveui = (const gchar*) sqlite3_column_text(stmt, 3);
	guint8 port = sqlite3_column_int(stmt, 4);
	const guint8* payload = sqlite3_column_blob(stmt, 5);
	gsize payloadlen = sqlite3_column_bytes(stmt, 5);

	struct uplink up = { .timestamp = timestamp, .appeui = appeui, .deveui =
			deveui, .port = port, .payload = payload, .payloadlen = payloadlen };

	((void (*)(const struct uplink*, void*)) callbackanddata->first)(&up,
			callbackanddata->second);
}

void database_uplinks_get(context_readonly* cntx, const char* deveui,
		void (*callback)(const struct uplink* uplink, void* data), void* data) {
	LOOKUPBYSTRING(cntx->dbcntx.getuplinks_dev, deveui,
			database_uplinks_get_rowcallback);
}

void database_uplinks_clean(context_readonly* cntx, guint64 timestamp) {
	sqlite3_bind_int64(cntx->dbcntx.cleanuplinks, 1, timestamp);
	database_stepuntilcomplete(cntx->dbcntx.cleanuplinks,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.cleanuplinks);
}

void database_downlink_add(context_readonly* cntx, struct downlink* downlink) {
	__sqlitegen_downlinks_add(cntx->dbcntx.insertdownlink, downlink);
	database_stepuntilcomplete(cntx->dbcntx.insertdownlink,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.insertdownlink);
}

void database_downlinks_clean(context_readonly* cntx, guint64 timestamp) {
	sqlite3_bind_int64(cntx->dbcntx.cleandownlinks, 1, timestamp);
	database_stepuntilcomplete(cntx->dbcntx.cleandownlinks,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.cleandownlinks);
}

static void database_downlinks_count_rowcallback(sqlite3_stmt* stmt, void* data) {
	int* rows = data;
	*rows = sqlite3_column_int(stmt, 0);
}

int database_downlinks_count(context_readonly* cntx, const char* appeui,
		const char* deveui) {
	int rows = 0;
	sqlite3_bind_text(cntx->dbcntx.countdownlinks, 1, appeui, -1,
	SQLITE_STATIC);
	sqlite3_bind_text(cntx->dbcntx.countdownlinks, 2, deveui, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.countdownlinks,
			database_downlinks_count_rowcallback, &rows);
	sqlite3_reset(cntx->dbcntx.countdownlinks);
	return rows;
}

// make a copy that is usable when sqlite has finished
static void database_downlinks_sqlitegen_callback(
		const struct downlink* downlink, void* data) {
	g_message("copying downlink %s", downlink->token);
	struct downlink* result = data;
	memcpy(result, downlink, sizeof(*result));
	result->appeui = g_strdup(downlink->appeui);
	result->deveui = g_strdup(downlink->deveui);
	result->payload = g_malloc(downlink->payloadlen);
	memcpy(result->payload, downlink->payload, downlink->payloadlen);
	result->token = g_strdup(downlink->token);
}

gboolean database_downlinks_get_first(context_readonly* cntx,
		const char* appeui, const char* deveui, struct downlink* downlink) {
	sqlite3_bind_text(cntx->dbcntx.downlinks_get_first, 1, appeui, -1,
	SQLITE_STATIC);
	sqlite3_bind_text(cntx->dbcntx.downlinks_get_first, 2, deveui, -1,
	SQLITE_STATIC);
	struct __sqlitegen_downlinks_rowcallback_callback cb = { .callback =
			database_downlinks_sqlitegen_callback, .data = downlink };
	database_stepuntilcomplete(cntx->dbcntx.downlinks_get_first,
			__sqlitegen_downlinks_rowcallback, &cb);
	sqlite3_reset(cntx->dbcntx.downlinks_get_first);
	return TRUE;
}

gboolean database_downlinks_delete_by_token(context_readonly* cntx,
		const char* token) {
	sqlite3_bind_text(cntx->dbcntx.downlinks_delete_by_token, 1, token, -1,
	SQLITE_STATIC);
	database_stepuntilcomplete(cntx->dbcntx.downlinks_delete_by_token,
	NULL, NULL);
	sqlite3_reset(cntx->dbcntx.downlinks_delete_by_token);
	return TRUE;
}
