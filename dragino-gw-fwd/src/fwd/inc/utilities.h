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

#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#ifndef SUCCESS
#define SUCCESS        1
#endif

#ifndef FAIL
#define FAIL           0
#endif

#ifndef MAX_TRY
#define MAX_TRY        3
#endif

/*!
 * \brief Returns the minimum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval minValue Minimum value
 */
#define MIN(a, b) ({ typeof(a) __a = (a); typeof(b) __b = (b); ((__a > __b) ? __b : __a);})

/*!
 * \brief Returns the maximum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval maxValue Maximum value
 */
#define MAX(a, b) ({ typeof(a) __a = (a); typeof(b) __b = (b); ((__a < __b) ? __b : __a);})

/*!
 * \brief swap value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 */
#define SWAP(a,b) do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/*!
 * \brief Returns 2 raised to the power of n
 *
 * \param [IN] n power value
 * \retval result of raising 2 to the power n
 */
#define POW2( n ) ( 1 << n )

/*!
 * \brief Initializes the pseudo random generator initial value
 *
 * \param [IN] seed Pseudo random generator initial value
 */
void lgw_rand(void);

/*!
 * \brief Computes a random number between min and max
 *
 * \param [IN] min range minimum value
 * \param [IN] max range maximum value
 * \retval random random value in range min..max
 */
int32_t lgw_randr( int32_t min, int32_t max );

/*!
 * \brief Copies size elements of src array to dst array
 *
 * \remark STM32 Standard memcpy function only works on pointers that are aligned
 *
 * \param [OUT] dst  Destination array
 * \param [IN]  src  Source array
 * \param [IN]  size Number of bytes to be copied
 */
void lgw_memcpy( uint8_t *dst, const uint8_t *src, uint16_t size );

/*!
 * \brief Copies size elements of src array to dst array reversing the byte order
 *
 * \param [OUT] dst  Destination array
 * \param [IN]  src  Source array
 * \param [IN]  size Number of bytes to be copied
 */
void lgw_memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size );

/*!
 * \brief Set size elements of dst array with value
 *
 * \remark STM32 Standard memset function only works on pointers that are aligned
 *
 * \param [OUT] dst   Destination array
 * \param [IN]  value Default value
 * \param [IN]  size  Number of bytes to be copied
 */
void lgw_memset( uint8_t *dst, uint8_t value, uint16_t size );

/*!
 * \brief Converts a nibble to an hexadecimal character
 *
 * \param [IN] a   Nibble to be converted
 * \retval hexChar Converted hexadecimal character
 */
int8_t nibble2hexchar( uint8_t a );

/*! 
 * \brief Begins critical section
 * 
 */
#define CRITICAL_SECTION_BEGIN( ) uint32_t mask; BoardCriticalSectionBegin( &mask )

/*!
 * \brief Ends critical section
 */
#define CRITICAL_SECTION_END( ) BoardCriticalSectionEnd( &mask )

/*!
 * \brief convert the strings to hex format
 *
 * \param [IN] dest
 * \param [out] src Pointer to a variable where the dest convert to
 */
void str2hex(uint8_t* dest, char* src, int len);

/*!
 * \brief convert the strings to hex format
 *
 * \param [IN] dest
 * \param [out] src Pointer to a variable where the dest convert to
 */
void hex2str(uint8_t* hex, uint8_t* str, uint8_t len);

/*!
 * \brief management pthread funciton 
 */

#if defined(PTHREAD_STACK_MIN)
# define LGW_STACKSIZE     MAX((((sizeof(void *) * 8 * 8) - 16) * 1024), PTHREAD_STACK_MIN)
# define LGW_STACKSIZE_LOW MAX((((sizeof(void *) * 8 * 2) - 16) * 1024), PTHREAD_STACK_MIN)
#else
# define LGW_STACKSIZE     (((sizeof(void *) * 8 * 8) - 16) * 1024)
# define LGW_STACKSIZE_LOW (((sizeof(void *) * 8 * 2) - 16) * 1024)
#endif

int lgw_background_stacksize(void);

#define LGW_BACKGROUND_STACKSIZE lgw_background_stacksize()

void lgw_register_thread(char *name);
void lgw_unregister_thread(void *id);

int lgw_pthread_create_stack(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *),
			     void *data, size_t stacksize, const char *file, const char *caller,
			     int line, const char *start_fn);

int lgw_pthread_create_detached_stack(pthread_t *thread, pthread_attr_t *attr, void*(*start_routine)(void *),
				 void *data, size_t stacksize, const char *file, const char *caller,
				 int line, const char *start_fn);

#define lgw_pthread_create(a, b, c, d) 				\
	lgw_pthread_create_stack(a, b, c, d,			\
		0, __FILE__, __FUNCTION__, __LINE__, #c)

#define lgw_pthread_create_detached(a, b, c, d)			\
	lgw_pthread_create_detached_stack(a, b, c, d,		\
		0, __FILE__, __FUNCTION__, __LINE__, #c)

#define lgw_pthread_create_background(a, b, c, d)		\
	lgw_pthread_create_stack(a, b, c, d,			\
		LGW_BACKGROUND_STACKSIZE,			\
		__FILE__, __FUNCTION__, __LINE__, #c)

#define lgw_pthread_create_detached_background(a, b, c, d)	\
	lgw_pthread_create_detached_stack(a, b, c, d,		\
		LGW_BACKGROUND_STACKSIZE,			\
		__FILE__, __FUNCTION__, __LINE__, #c)

/*!
 * \brief Get current thread ID
 * \return the ID if platform is supported, else -1
 */
int lgw_get_tid(void);

/*!
 * \brief Checks to see if value is within the given bounds
 *
 * \param v the value to check
 * \param min minimum lower bound (inclusive)
 * \param max maximum upper bound (inclusive)
 * \return 0 if value out of bounds, otherwise true (non-zero)
 */
#define IN_BOUNDS(v, min, max) ((v) >= (min)) && ((v) <= (max))

/*!
 * \brief Checks to see if value is within the bounds of the given array
 *
 * \param v the value to check
 * \param a the array to bound check
 * \return 0 if value out of bounds, otherwise true (non-zero)
 */
#define ARRAY_IN_BOUNDS(v, a) IN_BOUNDS((int) (v), 0, ARRAY_LEN(a) - 1)

#ifdef DO_CRASH
#define DO_CRASH_NORETURN attribute_noreturn
#else
#define DO_CRASH_NORETURN
#endif

void DO_CRASH_NORETURN __lgw_assert_failed(int condition, const char *condition_str,
	const char *file, int line, const char *function);

#ifdef LGW_DEVMODE
#define lgw_assert(a) _lgw_assert(a, # a, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define lgw_assert_return(a, ...) \
({ \
	if (__builtin_expect(!(a), 1)) { \
		_lgw_assert(0, # a, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
		return __VA_ARGS__; \
	}\
})
static void force_inline _lgw_assert(int condition, const char *condition_str, const char *file, int line, const char *function)
{
	if (__builtin_expect(!condition, 1)) {
		__lgw_assert_failed(condition, condition_str, file, line, function);
	}
}
#else
#define lgw_assert(a)
#define lgw_assert_return(a, ...) \
({ \
	if (__builtin_expect(!(a), 1)) { \
		return __VA_ARGS__; \
	}\
})
#endif

/*!
 * \brief Force a crash if DO_CRASH is defined.
 *
 * \note If DO_CRASH is not defined then the function returns.
 *
 * \return Nothing
 */
void DO_CRASH_NORETURN lgw_do_crash(void);

#endif // __UTILITIES_H__
