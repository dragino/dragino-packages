/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:
    Network server, receives UDP packets and dispatch

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

/*! \file
 * \brief locking-related definitions:
 * - lgw_mutex_t, lgw_rwlock_t and related functions;
 * - atomic arithmetic instructions;
 * - wrappers for channel locking.
 *
 * - See \ref LockDef
 */

/*! \page LockDef thread locking models
 *
 * This file provides different implementation of the functions,
 * depending on the platform, the use of DEBUG_THREADS, and the way
 * module-level mutexes are initialized.
 *
 *  - \b static: the mutex is assigned the value lgw_MUTEX_INIT_VALUE
 *        this is done at compile time, and is the way used on Linux.
 *        This method is not applicable to all platforms e.g. when the
 *        initialization needs that some code is run.
 *
 *  - \b through constructors: for each mutex, a constructor function is
 *        defined, which then runs when the program (or the module)
 *        starts. The problem with this approach is that there is a
 *        lot of code duplication (a new block of code is created for
 *        each mutex). Also, it does not prevent a user from declaring
 *        a global mutex without going through the wrapper macros,
 *        so sane programming practices are still required.
 */

#ifndef _LGW_LOCK_H
#define _LGW_LOCK_H

#include <pthread.h>
#include <time.h>
#include <sys/param.h>

#include "logger.h"
#include "compiler.h"

#define LGW_PTHREADT_NULL (pthread_t) -1
#define LGW_PTHREADT_STOP (pthread_t) -2

/* LGW REQUIRES recursive (not error checking) mutexes
   and will not run without them. */
#define PTHREAD_MUTEX_INIT_VALUE	PTHREAD_MUTEX_INITIALIZER
#define LGW_MUTEX_KIND			PTHREAD_MUTEX_RECURSIVE

#define LGW_LOCK_TRACK_INIT_VALUE { { NULL }, { 0 }, 0, { NULL }, { 0 }, PTHREAD_MUTEX_INIT_VALUE }

#define LGW_MUTEX_INIT_VALUE { PTHREAD_MUTEX_INIT_VALUE, NULL, {1, 0} }
#define LGW_MUTEX_INIT_VALUE_NOTRACKING { PTHREAD_MUTEX_INIT_VALUE, NULL, {0, 0} }

#define LGW_MAX_REENTRANCY 10

typedef struct lgw_mutex_info lgw_mutex_t;

int __lgw_pthread_mutex_init(int tracking, const char *filename, int lineno, const char *func, const char *mutex_name, lgw_mutex_t *t);
int __lgw_pthread_mutex_destroy(const char *filename, int lineno, const char *func, const char *mutex_name, lgw_mutex_t *t);
int __lgw_pthread_mutex_lock(const char *filename, int lineno, const char *func, const char* mutex_name, lgw_mutex_t *t);
int __lgw_pthread_mutex_trylock(const char *filename, int lineno, const char *func, const char* mutex_name, lgw_mutex_t *t);
int __lgw_pthread_mutex_unlock(const char *filename, int lineno, const char *func, const char *mutex_name, lgw_mutex_t *t);

#define lgw_mutex_init(pmutex)            __lgw_pthread_mutex_init(1, __FILE__, __LINE__, __PRETTY_FUNCTION__, #pmutex, pmutex)
#define lgw_mutex_destroy(a)              __lgw_pthread_mutex_destroy(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define lgw_mutex_lock(a)                 __lgw_pthread_mutex_lock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define lgw_mutex_unlock(a)               __lgw_pthread_mutex_unlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define lgw_mutex_trylock(a)              __lgw_pthread_mutex_trylock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)

