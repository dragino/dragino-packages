/*
SMS Server Tools 3
Copyright (C) 2006- Keijo Kasvi
http://smstools3.kekekasvi.com/

Based on SMS Server Tools 2 from Stefan Frings
http://www.meinemullemaus.de/
SMS Server Tools version 2 and below are Copyright (C) Stefan Frings.

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.
*/

#ifndef CHARSET_H
#define CHARSET_H

char logch_buffer[8192];

// Logging is not used externally, but it's placed to the end of source file.
void logch(char* format, ...);
char prch(char ch);

// Both functions return the size of the converted string
// max limits the number of characters to be written into
// destination
// size is the size of the source string
// max is the maximum size of the destination string
// The GSM character set contains 0x00 as a valid character

int gsm2iso(char* source, int size, char* destination, int max);

int iso_utf8_2gsm(char* source, int size, char* destination, int max);
int iso2utf8_file(FILE *fp, char *ascii, int userdatalength);

//int iso2gsm(char* source, int size, char* destination, int max);

//int unicode2sms(char* source, int size, char* destination, int max);

int decode_7bit_packed(char *text, char *dest, size_t size_dest);
int encode_7bit_packed(char *text, char *dest, size_t size_dest);

#ifndef USE_ICONV
int decode_ucs2(char *buffer, int len);
#else
int iconv_init(void);
size_t iconv_utf2ucs(char* buf, size_t len, size_t maxlen);
size_t iconv_ucs2utf(char* buf, size_t len, size_t maxlen);
size_t iconv_ucs2utf_chk(char *buf, size_t len, size_t maxlen);
int is_ascii_gsm(char* buf, size_t len);
#endif

#endif
