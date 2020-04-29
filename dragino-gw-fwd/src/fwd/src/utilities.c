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

/*!
 * \file
 * \brief Utility functions
 */

#include <stdlib.h>
#include <stdio.h>
#include "utilities.h"

#define RAND_LOCAL_MAX 2147483647L

static uint32_t next = 1;

int32_t lgw_rand( void )
{
    return ( ( next = next * 1103515245L + 12345L ) % RAND_LOCAL_MAX );
}

void lgw_srand( uint32_t seed )
{
    next = seed;
}


int32_t lgw_randr( int32_t min, int32_t max )
{
    return ( int32_t )lgw_rand( ) % ( max - min + 1 ) + min;
}

void lgw_memcpy( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    while( size-- )
    {
        *dst++ = *src++;
    }
}

void lgw_memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    dst = dst + ( size - 1 );
    while( size-- )
    {
        *dst-- = *src++;
    }
}

void lgw_memset( uint8_t *dst, uint8_t value, uint16_t size )
{
    while( size-- )
    {
        *dst++ = value;
    }
}

int8_t nibble2hexchar( uint8_t a )
{
    if( a < 10 )
    {
        return '0' + a;
    }
    else if( a < 16 )
    {
        return 'A' + ( a - 10 );
    }
    else
    {
        return '?';
    }
}

void str2hex(uint8_t* dest, char* src, int len) {
    int i;
    uint8_t ch1;
    uint8_t ch2;
    uint8_t ui1;
    uint8_t ui2;
    for(i = 0; i < len; i++) {
        ch1 = src[i*2];
        ch2 = src[i*2+1];
        ui1 = toupper(ch1) - 0x30;
        if (ui1 > 9)
            ui1 -= 7;
        ui2 = toupper(ch2) - 0x30;
        if (ui2 > 9)
            ui2 -= 7;
        dest[i] = ui1*16 + ui2;
    }
}

static uint8_t hex2int(char c) {
    /* 0x30 - 0x39 (0 - 9) 
     * 0x61 - 0x66 (a - f) 
     * 0x41 - 0x46 (A - F)
     * */
    if( c >= '0' && c <= '9') {
        return (uint8_t) (c - 0x30);
    } else if( c >= 'A' && c <= 'F') {
        return (uint8_t) (c - 0x37);
    } else if( c >= 'a' && c <= 'f') {
        return (uint8_t) (c - 0x57);
    } else {
        return 0;
    }
}

void hex2str(uint8_t* hex, uint8_t* str, uint8_t len) {
    int i = 0, j;
    uint8_t h, l;

    for(j = 0; j < len - 1; ) {
        h = hex2int(hex[j++]);
        l = hex2int(hex[j++]);
        str[i++] = (h<<4) | l;
    }
}

struct thr_arg {
	void *(*start_routine)(void *);
	void *data;
	char *name;
};

int lgw_background_stacksize(void)
{
#if !defined(LOW_MEMORY)
	return LGW_STACKSIZE;
#else
	return LGW_STACKSIZE_LOW;
#endif
}

int lgw_pthread_create_stack(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *),
			     void *data, size_t stacksize, const char *file, const char *caller,
			     int line, const char *start_fn)
{
#if !defined(LOW_MEMORY)
	struct thr_arg *a;
#endif

	if (!attr) {
		attr = lgw_alloca(sizeof(*attr));
		pthread_attr_init(attr);
	}

#if defined(__linux__) || defined(__FreeBSD__)
	/* On Linux and FreeBSD , pthread_attr_init() defaults to PTHREAD_EXPLICIT_SCHED,
	   which is kind of useless. Change this here to
	   PTHREAD_INHERIT_SCHED; that way the -p option to set realtime
	   priority will propagate down to new threads by default.
	   This does mean that callers cannot set a different priority using
	   PTHREAD_EXPLICIT_SCHED in the attr argument; instead they must set
	   the priority afterwards with pthread_setschedparam(). */
	if ((errno = pthread_attr_setinheritsched(attr, PTHREAD_INHERIT_SCHED)))
		lgw_log(LOG_WARNING, "pthread_attr_setinheritsched: %s\n", strerror(errno));
#endif

	if (!stacksize)
		stacksize = LGW_STACKSIZE;

	if ((errno = pthread_attr_setstacksize(attr, stacksize ? stacksize : LGW_STACKSIZE)))
		lgw_log(LOG_WARNING, "pthread_attr_setstacksize: %s\n", strerror(errno));

#if !defined(LOW_MEMORY)
	if ((a = lgw_malloc(sizeof(*a)))) {
		a->start_routine = start_routine;
		a->data = data;
		start_routine = dummy_start;
		if (lgw_asprintf(&a->name, "%-20s started at [%5d] %s %s()",
			     start_fn, line, file, caller) < 0) {
			a->name = NULL;
		}
		data = a;
	}
#endif /* !LOW_MEMORY */

	return pthread_create(thread, attr, start_routine, data); /* We're in lgw_pthread_create, so it's okay */
}


int lgw_pthread_create_detached_stack(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *),
			     void *data, size_t stacksize, const char *file, const char *caller,
			     int line, const char *start_fn)
{
	unsigned char attr_destroy = 0;
	int res;

	if (!attr) {
		attr = lgw_alloca(sizeof(*attr));
		pthread_attr_init(attr);
		attr_destroy = 1;
	}

	if ((errno = pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED)))
		lgw_log(LOG_WARNING, "pthread_attr_setdetachstate: %s\n", strerror(errno));

	res = lgw_pthread_create_stack(thread, attr, start_routine, data,
	                               stacksize, file, caller, line, start_fn);

	if (attr_destroy)
		pthread_attr_destroy(attr);

	return res;
}

int lgw_get_tid(void)
{
	int ret = -1;
	ret = pthread_self();
	return ret;
}

void DO_CRASH_NORETURN lgw_do_crash(void)
{
#if defined(DO_CRASH)
	abort();
	/*
	 * Just in case abort() doesn't work or something else super
	 * silly, and for Qwell's amusement.
	 */
	*((int *) 0) = 0;
#endif	/* defined(DO_CRASH) */
}

void DO_CRASH_NORETURN __lgw_assert_failed(int condition, const char *condition_str, const char *file, int line, const char *function)
{
	/*
	 * Attempt to put it into the logger, but hope that at least
	 * someone saw the message on stderr ...
	 */
	fprintf(stderr, "FRACK!, Failed assertion %s (%d) at line %d in %s of %s\n",
		condition_str, condition, line, function, file);
	lgw_log(__LOG_ERROR, file, line, function, "FRACK!, Failed assertion %s (%d)\n",
		condition_str, condition);

	/* Generate a backtrace for the assert */
	lgw_log_backtrace();

	/*
	 * Give the logger a chance to get the message out, just in case
	 * we abort(), or Asterisk crashes due to whatever problem just
	 * happened after we exit lgw_assert().
	 */
	usleep(1);
	lgw_do_crash();
}

