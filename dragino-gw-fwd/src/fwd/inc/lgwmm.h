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
 * \brief memory management routines
 *
 * This file should never be \#included directly, it is included
 * by fwd.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _LGW_MM_H
#define _LGW_MM_H

/* IWYU pragma: private, include "fwd.h" */

void *lgw_std_malloc(size_t size) attribute_malloc;
void *lgw_std_calloc(size_t nmemb, size_t size) attribute_malloc;
void *lgw_std_realloc(void *ptr, size_t size);
void lgw_std_free(void *ptr);

void *__lgw_calloc(size_t nmemb, size_t size, const char *file, int lineno, const char *func) attribute_malloc;
void *__lgw_malloc(size_t size, const char *file, int lineno, const char *func) attribute_malloc;
void __lgw_free(void *ptr, const char *file, int lineno, const char *func);
void *__lgw_realloc(void *ptr, size_t size, const char *file, int lineno, const char *func);
char *__lgw_strdup(const char *s, const char *file, int lineno, const char *func) attribute_malloc;
char *__lgw_strndup(const char *s, size_t n, const char *file, int lineno, const char *func) attribute_malloc;
int __lgw_asprintf(const char *file, int lineno, const char *func, char **strp, const char *format, ...) __attribute__((format(printf, 5, 6)));
int __lgw_vasprintf(char **strp, const char *format, va_list ap, const char *file, int lineno, const char *func) __attribute__((format(printf, 2, 0)));

/* Provide our own definition for lgw_free */

#define lgw_free(a)  \
    __lgw_free((a), __FILE__, __LINE__, __PRETTY_FUNCTION__);

/*!
 * \brief A wrapper for malloc()
 *
 * lgw_malloc() is a wrapper for malloc() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * The argument and return value are the same as malloc()
 */
#define lgw_malloc(len) \
	__lgw_malloc((len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for calloc()
 *
 * lgw_calloc() is a wrapper for calloc() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * The arguments and return value are the same as calloc()
 */
#define lgw_calloc(num, len) \
	__lgw_calloc((num), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for calloc() for use in cache pools
 *
 * lgw_calloc_cache() is a wrapper for calloc() that will generate an LGW log
 * message in the case that the allocation fails. When memory debugging is in use,
 * the memory allocated by this function will be marked as 'cache' so it can be
 * distinguished from normal memory allocations.
 *
 * The arguments and return value are the same as calloc()
 */
#define lgw_calloc_cache(num, len) \
	__lgw_calloc_cache((num), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for realloc()
 *
 * lgw_realloc() is a wrapper for realloc() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * The arguments and return value are the same as realloc()
 */
#define lgw_realloc(p, len) \
	__lgw_realloc((p), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for strdup()
 *
 * lgw_strdup() is a wrapper for strdup() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * lgw_strdup(), unlike strdup(), can safely accept a NULL argument. If a NULL
 * argument is provided, lgw_strdup will return NULL without generating any
 * kind of error log message.
 *
 * The argument and return value are the same as strdup()
 */
#define lgw_strdup(str) \
	__lgw_strdup((str), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for strndup()
 *
 * lgw_strndup() is a wrapper for strndup() that will generate an GW log
 * message in the case that the allocation fails.
 *
 * lgw_strndup(), unlike strndup(), can safely accept a NULL argument for the
 * string to duplicate. If a NULL argument is provided, lgw_strdup will return
 * NULL without generating any kind of error log message.
 *
 * The arguments and return value are the same as strndup()
 */
#define lgw_strndup(str, len) \
	__lgw_strndup((str), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief A wrapper for asprintf()
 *
 * lgw_asprintf() is a wrapper for asprintf() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * The arguments and return value are the same as asprintf()
 */
#define lgw_asprintf(ret, fmt, ...) \
	__lgw_asprintf(__FILE__, __LINE__, __PRETTY_FUNCTION__, (ret), (fmt), __VA_ARGS__)

/*!
 * \brief A wrapper for vasprintf()
 *
 * lgw_vasprintf() is a wrapper for vasprintf() that will generate an LGW log
 * message in the case that the allocation fails.
 *
 * The arguments and return value are the same as vasprintf()
 */
#define lgw_vasprintf(ret, fmt, ap) \
	__lgw_vasprintf((ret), (fmt), (ap), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
  \brief call __builtin_alloca to ensure we get gcc builtin semantics
  \param size The size of the buffer we want allocated

  This macro will attempt to allocate memory from the stack.  If it fails
  you won't get a NULL returned, but a SEGFAULT if you're lucky.
*/
#define lgw_alloca(size) __builtin_alloca(size)

#if !defined(lgw_strdupa) && defined(__GNUC__)
/*!
 * \brief duplicate a string in memory from the stack
 * \param s The string to duplicate
 *
 * This macro will duplicate the given string.  It returns a pointer to the stack
 * allocatted memory for the new string.
 */
#define lgw_strdupa(s)                                                    \
	(__extension__                                                    \
	({                                                                \
		const char *__old = (s);                                  \
		size_t __len = strlen(__old) + 1;                         \
		char *__new = __builtin_alloca(__len);                    \
		memcpy (__new, __old, __len);                             \
		__new;                                                    \
	}))
#endif

#else
#error "NEVER INCLUDE lgwmm.h DIRECTLY!!"
#endif /* _LGW_MM_H */

#ifdef __cplusplus
}
#endif
