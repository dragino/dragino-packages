/*!
 * \file      utilities.h
 *
 * \brief     Helper functions implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include <stdlib.h>
#include <stdio.h>
#include "utilities.h"

/*!
 * Redefinition of rand() and srand() standard C functions.
 * These functions are redefined in order to get the same behavior across
 * different compiler toolchains implementations.
 */
// Standard random functions redefinition start
#define RAND_LOCAL_MAX 2147483647L

static uint32_t next = 1;

int32_t rand1( void )
{
    return ( ( next = next * 1103515245L + 12345L ) % RAND_LOCAL_MAX );
}

void srand1( uint32_t seed )
{
    next = seed;
}
// Standard random functions redefinition end

int32_t randr( int32_t min, int32_t max )
{
    return ( int32_t )rand1( ) % ( max - min + 1 ) + min;
}

void memcpy1( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    while( size-- )
    {
        *dst++ = *src++;
    }
}

void memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    dst = dst + ( size - 1 );
    while( size-- )
    {
        *dst-- = *src++;
    }
}

void memset1( uint8_t *dst, uint8_t value, uint16_t size )
{
    while( size-- )
    {
        *dst++ = value;
    }
}

int8_t Nibble2HexChar( uint8_t a )
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

bool lgw_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
    int try_c = 0;
    do {
        try_c++;
        i = pthread_create(thread, attr, start_routine, arg);
    } while (i != 0 && try_c < MAX_TRY);

    if (i != 0) {
        *thread = (pthread_t)0;
        return false;
    }

    return true;
}


