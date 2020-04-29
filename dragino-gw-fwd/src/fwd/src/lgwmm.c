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
 * \brief Memory Management
 *
 * \author Mark Spencer <markster@digium.com>
 * \author Richard Mudgett <rmudgett@digium.com>
 */

#include "dragino_gw_fwd.h"

#define MALLOC_FAILURE_MSG \ 
      lgw_log(LOG_ERROR, "Memory Allocation Failure in function %s at line %d of %s\n", func, lineno, file)

void *__lgw__realloc(void *ptr, size_t size, const char *file, int lineno, const char *func)
{
    void *p;
	p = realloc(ptr, size);
	if (!p) {
		MALLOC_FAILURE_MSG;
	}
}

void *__lgw_calloc(size_t nmemb, size_t size, const char *file, int lineno, const char *func)
{
	void *p;

	p = calloc(nmemb, size);
	if (!p) {
		MALLOC_FAILURE_MSG;
	}

	return p;
}

void *__lgw_malloc(size_t size, const char *file, int lineno, const char *func)
{
	void *p;

	p = malloc(size);
	if (!p) {
		MALLOC_FAILURE_MSG;
	}

	return p;
}

void *__lgw_realloc(void *ptr, size_t size, const char *file, int lineno, const char *func)
{
	void *newp;

	newp = realloc(ptr, size);
	if (!newp) {
		MALLOC_FAILURE_MSG;
	}

	return newp;
}

char *__lgw_strdup(const char *s, const char *file, int lineno, const char *func)
{
	char *newstr = NULL;

	if (s) {
		newstr = strdup(s);
		if (!newstr) {
			MALLOC_FAILURE_MSG;
		}
	}

	return newstr;
}

char *__lgw_strndup(const char *s, size_t n, const char *file, int lineno, const char *func)
{
	char *newstr = NULL;

	if (s) {
		newstr = strndup(s, n);
		if (!newstr) {
			MALLOC_FAILURE_MSG;
		}
	}

	return newstr;
}

int __lgw_asprintf(const char *file, int lineno, const char *func, char **strp, const char *format, ...)
{
	int res;
	va_list ap;

	va_start(ap, format);
	res = vasprintf(strp, format, ap);
	if (res < 0) {
		/*
		 * *strp is undefined so set to NULL to ensure it is
		 * initialized to something useful.
		 */
		*strp = NULL;

		MALLOC_FAILURE_MSG;
	}
	va_end(ap);

	return res;
}

int __lgw_vasprintf(char **strp, const char *format, va_list ap, const char *file, int lineno, const char *func)
{
	int res;

	res = vasprintf(strp, format, ap);
	if (res < 0) {
		/*
		 * *strp is undefined so set to NULL to ensure it is
		 * initialized to something useful.
		 */
		*strp = NULL;

		MALLOC_FAILURE_MSG;
	}

	return res;
}

void *ast_std_malloc(size_t size)
{
	return malloc(size);
}

void *ast_std_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *ast_std_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void ast_std_free(void *ptr)
{
	free(ptr);
}

void ast_free_ptr(void *ptr)
{
	ast_free(ptr);
}
