/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*! \file
 *
 * \brief Gateway database Management
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sqlite3.h>

#include "gw.h"
#include "db.h"

#define MAX_DB_FIELD 256

pthread_mutex_t mx_dblock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dbcond;
static sqlite3 *GWDB;
static pthread_t syncthread;
static int doexit;
static int dosync;

static void db_sync(void);

#define DEFINE_SQL_STATEMENT(stmt,sql) static sqlite3_stmt *stmt; \
	const char stmt##_sql[] = sql;

DEFINE_SQL_STATEMENT(put_stmt, "INSERT OR REPLACE INTO gwdb (key, value) VALUES (?, ?)")
DEFINE_SQL_STATEMENT(get_stmt, "SELECT value FROM gwdb WHERE key=?")
DEFINE_SQL_STATEMENT(del_stmt, "DELETE FROM gwdb WHERE key=?")
DEFINE_SQL_STATEMENT(deltree_stmt, "DELETE FROM gwdb WHERE key || '/' LIKE ? || '/' || '%'")
DEFINE_SQL_STATEMENT(deltree_all_stmt, "DELETE FROM gwdb")
DEFINE_SQL_STATEMENT(gettree_stmt, "SELECT key, value FROM gwdb WHERE key || '/' LIKE ? || '/' || '%' ORDER BY key")
DEFINE_SQL_STATEMENT(gettree_all_stmt, "SELECT key, value FROM gwdb ORDER BY key")
DEFINE_SQL_STATEMENT(showkey_stmt, "SELECT key, value FROM gwdb WHERE key LIKE '%' || '/' || ? ORDER BY key")
DEFINE_SQL_STATEMENT(create_gwdb_stmt, "CREATE TABLE IF NOT EXISTS gwdb(key VARCHAR(256), value VARCHAR(256), PRIMARY KEY(key))")
DEFINE_SQL_STATEMENT(gettree_prefix_stmt, "SELECT key, value FROM gwdb WHERE key > ?1 AND key <= ?1 || X'ffff'")

static int init_stmt(sqlite3_stmt **stmt, const char *sql, size_t len)
{
	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_prepare(GWDB, sql, len, stmt, NULL) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't prepare statement '%s': %s\n", sql, sqlite3_errmsg(GWDB));
		pthread_mutex_unlock(&mx_dblock);
		return -1;
	}
	pthread_mutex_unlock(&mx_dblock);

	return 0;
}

/*! \internal
 * \brief Clean up the prepared SQLite3 statement
 * \note mx_dblock should already be locked prior to calling this method
 */
static int clean_stmt(sqlite3_stmt **stmt, const char *sql)
{
	if (sqlite3_finalize(*stmt) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't finalize statement '%s': %s\n", sql, sqlite3_errmsg(GWDB));
		*stmt = NULL;
		return -1;
	}
	*stmt = NULL;
	return 0;
}

/*! \internal
 * \brief Clean up all prepared SQLite3 statements
 * \note mx_dblock should already be locked prior to calling this method
 */
static void clean_statements(void)
{
	clean_stmt(&get_stmt, get_stmt_sql);
	clean_stmt(&del_stmt, del_stmt_sql);
	clean_stmt(&deltree_stmt, deltree_stmt_sql);
	clean_stmt(&deltree_all_stmt, deltree_all_stmt_sql);
	clean_stmt(&gettree_stmt, gettree_stmt_sql);
	clean_stmt(&gettree_all_stmt, gettree_all_stmt_sql);
	clean_stmt(&gettree_prefix_stmt, gettree_prefix_stmt_sql);
	clean_stmt(&showkey_stmt, showkey_stmt_sql);
	clean_stmt(&put_stmt, put_stmt_sql);
	clean_stmt(&create_gwdb_stmt, create_gwdb_stmt_sql);
}

static int init_statements(void)
{
	/* Don't initialize create_gwdb_statement here as the GWDB table needs to exist
	 * brefore these statements can be initialized */
	return init_stmt(&get_stmt, get_stmt_sql, sizeof(get_stmt_sql))
	|| init_stmt(&del_stmt, del_stmt_sql, sizeof(del_stmt_sql))
	|| init_stmt(&deltree_stmt, deltree_stmt_sql, sizeof(deltree_stmt_sql))
	|| init_stmt(&deltree_all_stmt, deltree_all_stmt_sql, sizeof(deltree_all_stmt_sql))
	|| init_stmt(&gettree_stmt, gettree_stmt_sql, sizeof(gettree_stmt_sql))
	|| init_stmt(&gettree_all_stmt, gettree_all_stmt_sql, sizeof(gettree_all_stmt_sql))
	|| init_stmt(&gettree_prefix_stmt, gettree_prefix_stmt_sql, sizeof(gettree_prefix_stmt_sql))
	|| init_stmt(&showkey_stmt, showkey_stmt_sql, sizeof(showkey_stmt_sql))
	|| init_stmt(&put_stmt, put_stmt_sql, sizeof(put_stmt_sql));
}

static int db_create_gwdb(void)
{
	int res = 0;

	if (!create_gwdb_stmt) {
		init_stmt(&create_gwdb_stmt, create_gwdb_stmt_sql, sizeof(create_gwdb_stmt_sql));
	}

	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_step(create_gwdb_stmt) != SQLITE_DONE) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't create GWDB table: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	}
	sqlite3_reset(create_gwdb_stmt);
	db_sync();
	pthread_mutex_unlock(&mx_dblock);

	return res;
}

static int db_open(void)
{
	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_open(LGW_DB_PATH, &GWDB) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Unable to open LGW database '%s': %s\n", dbname, sqlite3_errmsg(GWDB));
		sqlite3_close(GWDB);
		pthread_mutex_unlock(&mx_dblock);
		return -1;
	}

	pthread_mutex_unlock(&mx_dblock);

	return 0;
}

static int db_init(void)
{
	if (GWDB) {
		return 0;
	}

	if (db_open() || db_create_gwdb() || init_statements()) {
		return -1;
	}

	return 0;
}

/* We purposely don't lock around the sqlite3 call because the transaction
 * calls will be called with the database lock held. For any other use, make
 * sure to take the mx_dblock yourself. */
static int db_execute_sql(const char *sql, int (*callback)(void *, int, char **, char **), void *arg)
{
	char *errmsg = NULL;
	int res =0;

	if (sqlite3_exec(GWDB, sql, callback, arg, &errmsg) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Error executing SQL (%s): %s\n", sql, errmsg);
		sqlite3_free(errmsg);
		res = -1;
	}

	return res;
}

static int lgw_db_begin_transaction(void)
{
	return db_execute_sql("BEGIN TRANSACTION", NULL, NULL);
}

static int lgw_db_commit_transaction(void)
{
	return db_execute_sql("COMMIT", NULL, NULL);
}

static int lgw_db_rollback_transaction(void)
{
	return db_execute_sql("ROLLBACK", NULL, NULL);
}

int lgw_db_put(const char *family, const char *key, const char *value)
{
	char fullkey[MAX_DB_FIELD];
	size_t fullkey_len;
	int res = 0;

	if (strlen(family) + strlen(key) + 2 > sizeof(fullkey) - 1) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Family and key length must be less than %zu bytes\n", sizeof(fullkey) - 3);
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", family, key);

	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_bind_text(put_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't bind key to stmt: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	} else if (sqlite3_bind_text(put_stmt, 2, value, -1, SQLITE_STATIC) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't bind value to stmt: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	} else if (sqlite3_step(put_stmt) != SQLITE_DONE) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't execute statement: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	}

	sqlite3_reset(put_stmt);
	db_sync();
	pthread_mutex_unlock(&mx_dblock);

	return res;
}

/*!
 * \internal
 * \brief Get key value specified by family/key.
 *
 * Gets the value associated with the specified \a family and \a key, and
 * stores it, either into the fixed sized buffer specified by \a buffer
 * and \a bufferlen, or as a heap allocated string if \a bufferlen is -1.
 *
 * \retval -1 An error occurred
 * \retval 0 Success
 */
static int db_get_common(const char *family, const char *key, char **buffer, int bufferlen)
{
	const unsigned char *result;
	char fullkey[MAX_DB_FIELD];
	size_t fullkey_len;
	int res = 0;

	if (strlen(family) + strlen(key) + 2 > sizeof(fullkey) - 1) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Family and key length must be less than %zu bytes\n", sizeof(fullkey) - 3);
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", family, key);

	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_bind_text(get_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't bind key to stmt: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	} else if (sqlite3_step(get_stmt) != SQLITE_ROW) {
		lgw_log(LOG_DEBUG, "DEBUG~ [db] Unable to find key '%s' in family '%s'\n", key, family);
		res = -1;
	} else if (!(result = sqlite3_column_text(get_stmt, 0))) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't get value\n");
		res = -1;
	} else {
		const char *value = (const char *) result;

		if (bufferlen == -1) {
			*buffer = strdup(value);
		} else {
			strncpy(*buffer, value, bufferlen);
		}
	}
	sqlite3_reset(get_stmt);
	pthread_mutex_unlock(&mx_dblock);

	return res;
}

bool lgw_db_key_exist(const char *key) {
	pthread_mutex_lock(&dblock);
    if (!lgw_strlen_zero(key) && (sqlite3_bind_text(showkey_stmt, 1, key, -1, SQLITE_STATIC) != SQLITE_OK)) {
        lgw_log(LOG_WARNING, "WARNING~ [db] Could bind %s to stmt: %s\n", a->argv[2], sqlite3_errmsg(GWDB));
        sqlite3_reset(showkey_stmt);
        pthread_mutex_unlock(&dblock);
        return false;
    }

    if (sqlite3_step(showkey_stmt) == SQLITE_ROW) {
        sqlite3_reset(showkey_stmt);
        pthread_mutex_unlock(&dblock);
        return true;
    }

    sqlite3_reset(showkey_stmt);
    pthread_mutex_unlock(&dblock);
    return false;
}

int lgw_db_get(const char *family, const char *key, char *value, int valuelen)
{
	lgw_assert(value != NULL);

	/* Make sure we initialize */
	value[0] = 0;

	return db_get_common(family, key, &value, valuelen);
}

int lgw_db_get_allocated(const char *family, const char *key, char **out)
{
	*out = NULL;

	return db_get_common(family, key, out, -1);
}

int lgw_db_del(const char *family, const char *key)
{
	char fullkey[MAX_DB_FIELD];
	size_t fullkey_len;
	int res = 0;

	if (strlen(family) + strlen(key) + 2 > sizeof(fullkey) - 1) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Family and key length must be less than %zu bytes\n", sizeof(fullkey) - 3);
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", family, key);

	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_bind_text(del_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't bind key to stmt: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	} else if (sqlite3_step(del_stmt) != SQLITE_DONE) {
		lgw_log(LOG_DEBUG, "DEBUG~ [db] Unable to find key '%s' in family '%s'\n", key, family);
		res = -1;
	}
	sqlite3_reset(del_stmt);
	db_sync();
	pthread_mutex_unlock(&mx_dblock);

	return res;
}

int lgw_db_deltree(const char *family, const char *keytree)
{
	sqlite3_stmt *stmt = deltree_stmt;
	char prefix[MAX_DB_FIELD];
	int res = 0;

	if (!lgw_strlen_zero(family)) {
		if (!lgw_strlen_zero(keytree)) {
			/* Family and key tree */
			snprintf(prefix, sizeof(prefix), "/%s/%s", family, keytree);
		} else {
			/* Family only */
			snprintf(prefix, sizeof(prefix), "/%s", family);
		}
	} else {
		prefix[0] = '\0';
		stmt = deltree_all_stmt;
	}

	pthread_mutex_lock(&mx_dblock);
	if (!lgw_strlen_zero(prefix) && (sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_STATIC) != SQLITE_OK)) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Could bind %s to stmt: %s\n", prefix, sqlite3_errmsg(GWDB));
		res = -1;
	} else if (sqlite3_step(stmt) != SQLITE_DONE) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Couldn't execute stmt: %s\n", sqlite3_errmsg(GWDB));
		res = -1;
	}
	res = sqlite3_changes(GWDB);
	sqlite3_reset(stmt);
	db_sync();
	pthread_mutex_unlock(&mx_dblock);

	return res;
}

static struct lgw_db_entry *db_gettree_common(sqlite3_stmt *stmt)
{
	struct lgw_db_entry *head = NULL, *prev = NULL, *cur;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *key, *value;
		size_t key_len, value_len;

		key   = (const char *) sqlite3_column_text(stmt, 0);
		value = (const char *) sqlite3_column_text(stmt, 1);

		if (!key || !value) {
			break;
		}

		key_len = strlen(key);
		value_len = strlen(value);

		cur = malloc(sizeof(*cur) + key_len + value_len + 2);
		if (!cur) {
			break;
		}

		cur->next = NULL;
		cur->key = cur->data + value_len + 1;
		memcpy(cur->data, value, value_len + 1);
		memcpy(cur->key, key, key_len + 1);

		if (prev) {
			prev->next = cur;
		} else {
			head = cur;
		}
		prev = cur;
	}

	return head;
}

struct lgw_db_entry *lgw_db_gettree(const char *family, const char *keytree)
{
	char prefix[MAX_DB_FIELD];
	sqlite3_stmt *stmt = gettree_stmt;
	size_t res = 0;
	struct lgw_db_entry *ret;

	if (!lgw_strlen_zero(family)) {
		if (!lgw_strlen_zero(keytree)) {
			/* Family and key tree */
			res = snprintf(prefix, sizeof(prefix), "/%s/%s", family, keytree);
		} else {
			/* Family only */
			res = snprintf(prefix, sizeof(prefix), "/%s", family);
		}

		if (res >= sizeof(prefix)) {
			lgw_log(LOG_WARNING, "WARNING~ [db] Requested prefix is too long: %s\n", keytree);
			return NULL;
		}
	} else {
		prefix[0] = '\0';
		stmt = gettree_all_stmt;
	}

	pthread_mutex_lock(&mx_dblock);
	if (res && (sqlite3_bind_text(stmt, 1, prefix, res, SQLITE_STATIC) != SQLITE_OK)) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Could not bind %s to stmt: %s\n", prefix, sqlite3_errmsg(GWDB));
		sqlite3_reset(stmt);
		pthread_mutex_unlock(&mx_dblock);
		return NULL;
	}

	ret = db_gettree_common(stmt);
	sqlite3_reset(stmt);
	pthread_mutex_unlock(&mx_dblock);

	return ret;
}

struct lgw_db_entry *lgw_db_gettree_by_prefix(const char *family, const char *key_prefix)
{
	char prefix[MAX_DB_FIELD];
	size_t res;
	struct lgw_db_entry *ret;

	res = snprintf(prefix, sizeof(prefix), "/%s/%s", family, key_prefix);
	if (res >= sizeof(prefix)) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Requested key prefix is too long: %s\n", key_prefix);
		return NULL;
	}

	pthread_mutex_lock(&mx_dblock);
	if (sqlite3_bind_text(gettree_prefix_stmt, 1, prefix, res, SQLITE_STATIC) != SQLITE_OK) {
		lgw_log(LOG_WARNING, "WARNING~ [db] Could not bind %s to stmt: %s\n", prefix, sqlite3_errmsg(GWDB));
		sqlite3_reset(gettree_prefix_stmt);
		pthread_mutex_unlock(&mx_dblock);
		return NULL;
	}

	ret = db_gettree_common(gettree_prefix_stmt);
	sqlite3_reset(gettree_prefix_stmt);
	pthread_mutex_unlock(&mx_dblock);

	return ret;
}

void lgw_db_freetree(struct lgw_db_entry *dbe)
{
	struct lgw_db_entry *last;
	while (dbe) {
		last = dbe;
		dbe = dbe->next;
		lgw_free(last);
	}
}

/*!
 * \internal
 * \brief Signal the GWDB sync thread to do its thing.
 *
 * \note mx_dblock is assumed to be held when calling this function.
 */
static void db_sync(void)
{
	dosync = 1;
	pthread_cond_signal(&dbcond);
}

/*!
 * \internal
 * \brief GWDB sync thread
 *
 * This thread is in charge of syncing GWDB to disk after a change.
 * By pushing it off to this thread to take care of, this I/O bound operation
 * will not block other threads from performing other critical processing.
 * If changes happen rapidly, this thread will also ensure that the sync
 * operations are rate limited.
 */
static void *db_sync_thread(void *data)
{
	pthread_mutex_lock(&mx_dblock);
	lgw_db_begin_transaction();
	for (;;) {
		/* If dosync is set, db_sync() was called during sleep(1),
		 * and the pending transaction should be committed.
		 * Otherwise, block until db_sync() is called.
		 */
		while (!dosync) {
			pthread_cond_wait(&dbcond, &mx_dblock);
		}
		dosync = 0;
		if (lgw_db_commit_transaction()) {
			lgw_db_rollback_transaction();
		}
		if (doexit) {
			pthread_mutex_unlock(&mx_dblock);
			break;
		}
		lgw_db_begin_transaction();
		pthread_mutex_unlock(&mx_dblock);
		sleep(1);
		pthread_mutex_lock(&mx_dblock);
	}

	return NULL;
}

/*!
 * \internal
 * \brief Clean up resources on shutdown
 */
static void gwdb_atexit(void)
{
	/* Set doexit to 1 to kill thread. db_sync must be called with
	 * mutex held. */
	pthread_mutex_lock(&mx_dblock);
	doexit = 1;
	db_sync();
	pthread_mutex_unlock(&mx_dblock);

	pthread_join(syncthread, NULL);
	pthread_mutex_lock(&mx_dblock);
	clean_statements();
	if (sqlite3_close(GWDB) == SQLITE_OK) {
		GWDB = NULL;
	}
	pthread_mutex_unlock(&mx_dblock);
}

int lgw_db_init(void)
{
	pthread_cond_init(&dbcond, NULL);

	if (db_init()) {
		return -1;
	}

	if (pthread_create(&syncthread, NULL, db_sync_thread, NULL)) {
		return -1;
	}

	lgw_register_atexit(gwdb_atexit);
	return 0;
}
