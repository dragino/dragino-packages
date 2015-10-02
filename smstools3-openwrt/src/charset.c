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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <ctype.h>
#ifdef USE_ICONV
#include <iconv.h>
#include <errno.h>
#endif
#include "charset.h"
#include "logging.h"
#include "smsd_cfg.h"
#include "pdu.h"
#include "extras.h"

// For incoming character 0x24 conversion:
// Change this if other than Euro character is wanted, like '?' or '$'.
#define GSM_CURRENCY_SYMBOL_TO_ISO 0xA4

// For incoming character 0x09 conversion:
// (some reference: ftp://www.unicode.org/Public/MAPPINGS/ETSI/GSM0338.TXT)
// Uncomment this if you want that C-CEDILLA is represented as small c-cedilla:
//#define INCOMING_SMALL_C_CEDILLA

// iso = ISO8859-15 (you might change the table to any other 8-bit character set)
// sms = sms character set used by mobile phones

//                  iso   sms
char charset[] = { '@' , 0x00, // COMMERCIAL AT
		   0xA3, 0x01, // POUND SIGN
		   '$' , 0x02, // DOLLAR SIGN
		   0xA5, 0x03, // YEN SIGN
		   0xE8, 0x04, // LATIN SMALL LETTER E WITH GRAVE
		   0xE9, 0x05, // LATIN SMALL LETTER E WITH ACUTE
		   0xF9, 0x06, // LATIN SMALL LETTER U WITH GRAVE
		   0xEC, 0x07, // LATIN SMALL LETTER I WITH GRAVE
		   0xF2, 0x08, // LATIN SMALL LETTER O WITH GRAVE

#ifdef INCOMING_SMALL_C_CEDILLA
		   0xE7, 0x09, // LATIN SMALL LETTER C WITH CEDILLA
#else
		   0xC7, 0x09, // LATIN CAPITAL LETTER C WITH CEDILLA
#endif

		   0x0A, 0x0A, // LF
		   0xD8, 0x0B, // LATIN CAPITAL LETTER O WITH STROKE
		   0xF8, 0x0C, // LATIN SMALL LETTER O WITH STROKE
		   0x0D, 0x0D, // CR
		   0xC5, 0x0E, // LATIN CAPITAL LETTER A WITH RING ABOVE
		   0xE5, 0x0F, // LATIN SMALL LETTER A WITH RING ABOVE

// ISO8859-7, Capital greek characters
//		   0xC4, 0x10,
//		   0x5F, 0x11,
//		   0xD6, 0x12,
//		   0xC3, 0x13,
//		   0xCB, 0x14,
//		   0xD9, 0x15,
//		   0xD0, 0x16,
//		   0xD8, 0x17,
//		   0xD3, 0x18,
//		   0xC8, 0x19,
//		   0xCE, 0x1A,

// ISO8859-1, ISO8859-15
		   0x81, 0x10, // GREEK CAPITAL LETTER DELTA
		   0x5F, 0x11, // LOW LINE
		   0x82, 0x12, // GREEK CAPITAL LETTER PHI
		   0x83, 0x13, // GREEK CAPITAL LETTER GAMMA
		   0x84, 0x14, // GREEK CAPITAL LETTER LAMDA
		   0x85, 0x15, // GREEK CAPITAL LETTER OMEGA
		   0x86, 0x16, // GREEK CAPITAL LETTER PI
		   0x87, 0x17, // GREEK CAPITAL LETTER PSI
		   0x88, 0x18, // GREEK CAPITAL LETTER SIGMA
		   0x89, 0x19, // GREEK CAPITAL LETTER THETA
		   0x8A, 0x1A, // GREEK CAPITAL LETTER XI

		   0x1B, 0x1B, // ESC
		   0xC6, 0x1C, // LATIN CAPITAL LETTER AE
		   0xE6, 0x1D, // LATIN SMALL LETTER AE
		   0xDF, 0x1E, // LATIN SMALL LETTER SHARP S
		   0xC9, 0x1F, // LATIN CAPITAL LETTER E WITH ACUTE
		   ' ' , 0x20, // SPACE
		   '!' , 0x21, // EXCLAMATION MARK
		   0x22, 0x22, // QUOTATION MARK
		   '#' , 0x23, // NUMBER SIGN

                   // GSM character 0x24 is a "currency symbol".
                   // This character is never sent. Incoming character is converted without conversion tables.

		   '%' , 0x25, // PERSENT SIGN
		   '&' , 0x26, // AMPERSAND
		   0x27, 0x27, // APOSTROPHE
		   '(' , 0x28, // LEFT PARENTHESIS
		   ')' , 0x29, // RIGHT PARENTHESIS
		   '*' , 0x2A, // ASTERISK
		   '+' , 0x2B, // PLUS SIGN
		   ',' , 0x2C, // COMMA
		   '-' , 0x2D, // HYPHEN-MINUS
		   '.' , 0x2E, // FULL STOP
		   '/' , 0x2F, // SOLIDUS
		   '0' , 0x30, // DIGIT 0...9
		   '1' , 0x31,
		   '2' , 0x32,
		   '3' , 0x33,
		   '4' , 0x34,
		   '5' , 0x35,
		   '6' , 0x36,
		   '7' , 0x37,
		   '8' , 0x38,
		   '9' , 0x39,
		   ':' , 0x3A, // COLON
		   ';' , 0x3B, // SEMICOLON
		   '<' , 0x3C, // LESS-THAN SIGN
		   '=' , 0x3D, // EQUALS SIGN
		   '>' , 0x3E, // GREATER-THAN SIGN
		   '?' , 0x3F, // QUESTION MARK
		   0xA1, 0x40, // INVERTED EXCLAMATION MARK
		   'A' , 0x41, // LATIN CAPITAL LETTER A...Z
		   'B' , 0x42,
		   'C' , 0x43,
		   'D' , 0x44,
		   'E' , 0x45,
		   'F' , 0x46,
		   'G' , 0x47,
		   'H' , 0x48,
		   'I' , 0x49,
		   'J' , 0x4A,
		   'K' , 0x4B,
		   'L' , 0x4C,
		   'M' , 0x4D,
		   'N' , 0x4E,
		   'O' , 0x4F,
		   'P' , 0x50,
		   'Q' , 0x51,
		   'R' , 0x52,
		   'S' , 0x53,
		   'T' , 0x54,
		   'U' , 0x55,
		   'V' , 0x56,
		   'W' , 0x57,
		   'X' , 0x58,
		   'Y' , 0x59,
		   'Z' , 0x5A,
		   0xC4, 0x5B, // LATIN CAPITAL LETTER A WITH DIAERESIS
		   0xD6, 0x5C, // LATIN CAPITAL LETTER O WITH DIAERESIS
		   0xD1, 0x5D, // LATIN CAPITAL LETTER N WITH TILDE
		   0xDC, 0x5E, // LATIN CAPITAL LETTER U WITH DIAERESIS
		   0xA7, 0x5F, // SECTION SIGN
		   0xBF, 0x60, // INVERTED QUESTION MARK
		   'a' , 0x61, // LATIN SMALL LETTER A...Z
		   'b' , 0x62,
		   'c' , 0x63,
		   'd' , 0x64,
		   'e' , 0x65,
		   'f' , 0x66,
		   'g' , 0x67,
		   'h' , 0x68,
		   'i' , 0x69,
		   'j' , 0x6A,
		   'k' , 0x6B,
		   'l' , 0x6C,
		   'm' , 0x6D,
		   'n' , 0x6E,
		   'o' , 0x6F,
		   'p' , 0x70,
		   'q' , 0x71,
		   'r' , 0x72,
		   's' , 0x73,
		   't' , 0x74,
		   'u' , 0x75,
		   'v' , 0x76,
		   'w' , 0x77,
		   'x' , 0x78,
		   'y' , 0x79,
		   'z' , 0x7A,
		   0xE4, 0x7B, // LATIN SMALL LETTER A WITH DIAERESIS
		   0xF6, 0x7C, // LATIN SMALL LETTER O WITH DIAERESIS
		   0xF1, 0x7D, // LATIN SMALL LETTER N WITH TILDE
		   0xFC, 0x7E, // LATIN SMALL LETTER U WITH DIAERESIS
		   0xE0, 0x7F, // LATIN SMALL LETTER A WITH GRAVE

// Moved to the special char handling:
//		   0x60, 0x27, // GRAVE ACCENT
//                   0xE1, 0x61,  // replacement for accented a
//                   0xED, 0x69,  // replacement for accented i
//                   0xF3, 0x6F,  // replacement for accented o
//                   0xFA, 0x75,  // replacement for accented u

		   0   , 0     // End marker
		 };

// Extended characters. In GSM they are preceeded by 0x1B.

char ext_charset[] = { 0x0C, 0x0A, // <FF>
		       '^' , 0x14, // CIRCUMFLEX ACCENT
		       '{' , 0x28, // LEFT CURLY BRACKET
		       '}' , 0x29, // RIGHT CURLY BRACKET
		       '\\', 0x2F, // REVERSE SOLIDUS
		       '[' , 0x3C, // LEFT SQUARE BRACKET
		       '~' , 0x3D, // TILDE
		       ']' , 0x3E, // RIGHT SQUARE BRACKET
		       0x7C, 0x40, // VERTICAL LINE
		       0xA4, 0x65, // EURO SIGN
		       0   , 0     // End marker
	             };


// This table is used for outgoing (to GSM) conversion only:

char iso_8859_15_chars[] =
{
	0x60, 0x27, // GRAVE ACCENT --> APOSTROPHE
	0xA0, 0x20, // NO-BREAK SPACE --> SPACE
	0xA2, 0x63, // CENT SIGN --> c
	0xA6, 0x53, // LATIN CAPITAL LETTER S WITH CARON --> S
	0xA8, 0x73, // LATIN SMALL LETTER S WITH CARON --> s
	0xA9, 0x43, // COPYRIGHT SIGN --> C
	0xAA, 0x61, // FEMININE ORDINAL INDICATOR --> a
	0xAB, 0x3C, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK --> <
	0xAC, 0x2D, // NOT SIGN --> -
	0xAD, 0x2D, // SOFT HYPHEN --> -
	0xAE, 0x52, // REGISTERED SIGN --> R
	0xAF, 0x2D, // MACRON --> -
	0xB0, 0x6F, // DEGREE SIGN --> o
	0xB1, 0x2B, // PLUS-MINUS SIGN --> +
	0xB2, 0x32, // SUPERSCRIPT TWO --> 2
	0xB3, 0x33, // SUPERSCRIPT THREE --> 3
	0xB4, 0x5A, // LATIN CAPITAL LETTER Z WITH CARON --> Z
	0xB5, 0x75, // MICRO SIGN --> u
	0xB6, 0x49, // PILCROW SIGN --> I
	0xB7, 0x2E, // MIDDLE DOT --> .
	0xB8, 0x7A, // LATIN SMALL LETTER Z WITH CARON --> z
	0xB9, 0x31, // SUPERSCRIPT ONE --> 1
	0xBA, 0x6F, // MASCULINE ORDINAL INDICATOR --> o
	0xBB, 0x3E, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK --> >
	0xBC, 0x4F, // LATIN CAPITAL LIGATURE OE --> O
	0xBD, 0x6F, // LATIN SMALL LIGATURE OE --> o
	0xBE, 0x59, // LATIN CAPITAL LETTER Y WITH DIAERESIS --> Y
	0xC0, 0x41, // LATIN CAPITAL LETTER A WITH GRAVE --> A
	0xC1, 0x41, // LATIN CAPITAL LETTER A WITH ACUTE --> A
	0xC2, 0x41, // LATIN CAPITAL LETTER A WITH CIRCUMFLEX --> A
	0xC3, 0x41, // LATIN CAPITAL LETTER A WITH TILDE --> A
	0xC7, 0x09, // LATIN CAPITAL LETTER C WITH CEDILLA --> 0x09 (LATIN CAPITAL LETTER C WITH CEDILLA)
	0xC8, 0x45, // LATIN CAPITAL LETTER E WITH GRAVE --> E
	0xCA, 0x45, // LATIN CAPITAL LETTER E WITH CIRCUMFLEX --> E
	0xCB, 0x45, // LATIN CAPITAL LETTER E WITH DIAERESIS --> E
	0xCC, 0x49, // LATIN CAPITAL LETTER I WITH GRAVE --> I
	0xCD, 0x49, // LATIN CAPITAL LETTER I WITH ACUTE --> I
	0xCE, 0x49, // LATIN CAPITAL LETTER I WITH CIRCUMFLEX --> I
	0xCF, 0x49, // LATIN CAPITAL LETTER I WITH DIAERESIS --> I
	0xD0, 0x44, // LATIN CAPITAL LETTER ETH --> D
	0xD2, 0x4F, // LATIN CAPITAL LETTER O WITH GRAVE --> O
	0xD3, 0x4F, // LATIN CAPITAL LETTER O WITH ACUTE --> O
	0xD4, 0x4F, // LATIN CAPITAL LETTER O WITH CIRCUMFLEX --> O
	0xD5, 0x4F, // LATIN CAPITAL LETTER O WITH TILDE --> O
	0xD7, 0x78, // MULTIPLICATION SIGN --> x
	0xD9, 0x55, // LATIN CAPITAL LETTER U WITH GRAVE --> U
	0xDA, 0x55, // LATIN CAPITAL LETTER U WITH ACUTE --> U
	0xDB, 0x55, // LATIN CAPITAL LETTER U WITH CIRCUMFLEX --> U
	0xDD, 0x59, // LATIN CAPITAL LETTER Y WITH ACUTE --> Y
	0xDE, 0x62, // LATIN CAPITAL LETTER THORN --> b
	0xE1, 0x61, // LATIN SMALL LETTER A WITH ACUTE --> a
	0xE2, 0x61, // LATIN SMALL LETTER A WITH CIRCUMFLEX --> a
	0xE3, 0x61, // LATIN SMALL LETTER A WITH TILDE --> a
	0xE7, 0x09, // LATIN SMALL LETTER C WITH CEDILLA --> LATIN CAPITAL LETTER C WITH CEDILLA
	0xEA, 0x65, // LATIN SMALL LETTER E WITH CIRCUMFLEX --> e
	0xEB, 0x65, // LATIN SMALL LETTER E WITH DIAERESIS --> e
	0xED, 0x69, // LATIN SMALL LETTER I WITH ACUTE --> i
	0xEE, 0x69, // LATIN SMALL LETTER I WITH CIRCUMFLEX --> i
	0xEF, 0x69, // LATIN SMALL LETTER I WITH DIAERESIS --> i
	0xF0, 0x6F, // LATIN SMALL LETTER ETH --> o
	0xF3, 0x6F, // LATIN SMALL LETTER O WITH ACUTE --> o
	0xF4, 0x6F, // LATIN SMALL LETTER O WITH CIRCUMFLEX --> o
	0xF5, 0x6F, // LATIN SMALL LETTER O WITH TILDE --> o
	0xF7, 0x2F, // DIVISION SIGN --> / (SOLIDUS)
	0xFA, 0x75, // LATIN SMALL LETTER U WITH ACUTE --> u
	0xFB, 0x75, // LATIN SMALL LETTER U WITH CIRCUMFLEX --> u
	0xFD, 0x79, // LATIN SMALL LETTER Y WITH ACUTE --> y
	0xFE, 0x62, // LATIN SMALL LETTER THORN --> b
	0xFF, 0x79, // LATIN SMALL LETTER Y WITH DIAERESIS --> y

	0   , 0
};

#ifdef USE_ICONV
static iconv_t iconv4ucs;	// UCS2->UTF8 descriptor
static iconv_t iconv2ucs;	// UTF8->UCS2 descriptor
#endif

int special_char2gsm(char ch, char *newch)
{
  int table_row = 0;
  char *table = iso_8859_15_chars;

  while (table[table_row *2])
  {
    if (table[table_row *2] == ch)
    {
      if (newch)
        *newch = table[table_row *2 +1];
      return 1;
    }
    table_row++;
  }

  return 0;
}

// Return value:
// 0 = ch not found.
// 1 = ch found from normal table
// 2 = ch found from extended table
int char2gsm(char ch, char *newch)
{
  int result = 0;
  int table_row;

  // search in normal translation table
  table_row=0;
  while (charset[table_row*2])
  {
    if (charset[table_row*2] == ch)
    {
      if (newch)
        *newch = charset[table_row*2+1];
      result = 1;
      break;
    }
    table_row++;
  }

  // if not found in normal table, then search in the extended table
  if (result == 0)
  {
    table_row=0;
    while (ext_charset[table_row*2])
    {
      if (ext_charset[table_row*2] == ch)
      {
        if (newch)
          *newch = ext_charset[table_row*2+1];
        result = 2;
        break;
      }
      table_row++;
    }
  }

  return result;
}

int gsm2char(char ch, char *newch, int which_table)
{
  int table_row = 0;
  char *table;

  if (which_table == 1)
    table = charset;
  else if (which_table == 2)
    table = ext_charset;
  else
    return 0;

  while (table[table_row *2])
  {
    if (table[table_row *2 +1] == ch)
    {
      *newch = table[table_row *2];
      return 1;
    }
    table_row++;
  }

  return 0;
}

int iso_utf8_2gsm(char* source, int size, char* destination, int max)
{
  int source_count=0;
  int dest_count=0;
  int found=0;
  char newch;
  char logtmp[51];
  char tmpch;

  destination[dest_count]=0;
  if (source==0 || size <= 0)
    return 0;

#ifdef DEBUGMSG
  log_charconv = 1;
#endif

  if (log_charconv)
  {
    *logch_buffer = 0;
    logch("!! iso_utf8_2gsm(source=%.*s, size=%i)", size, source, size);
    logch(NULL);
  }

  // Convert each character until end of string
  while (source_count<size && dest_count<max)
  {
    found = char2gsm(source[source_count], &newch);
    if (found == 2)
    {
      if (dest_count >= max -2)
        break;
      destination[dest_count++] = 0x1B;
    }
    if (found >= 1)
    {
      destination[dest_count++] = newch;
      if (log_charconv)
      {
        sprintf(logtmp, "%02X[%c]", (unsigned char)source[source_count], prch(source[source_count]));
        if (found > 1 || source[source_count] != newch)
        {
          sprintf(strchr(logtmp, 0), "->%s%02X", (found == 2)? "Esc-" : "", (unsigned char)newch);
          if (gsm2char(newch, &tmpch, found))
            sprintf(strchr(logtmp, 0), "[%c]", tmpch);
        }
        logch("%s ", logtmp);
      }
    }

    if (found == 0 && outgoing_utf8)
    {
      // ASCII and UTF-8 table: http://members.dslextreme.com/users/kkj/webtools/ascii_utf8_table.html
      // Good converter: http://www.macchiato.com/unicode/convert.html
      unsigned int c;
      int iterations = 0;
      // 3.1beta7: If UTF-8 decoded character is not found from tables, decoding is ignored:
      int saved_source_count = source_count;
      char sourcechars[51];

      c = source[source_count];
      if (log_charconv)
        sprintf(sourcechars, "%02X", (unsigned char)source[source_count]);

      // 3.1beta7: Check if there is enough characters left.
      // Following bytes in UTF-8 should begin with 10xx xxxx
      // which means 0x80 ... 0xBF
      if (((c & 0xFF) >= 0xC2 && (c & 0xFF) <= 0xC7) ||
          ((c & 0xFF) >= 0xD0 && (c & 0xFF) <= 0xD7))
      {
        if (source_count < size -1 && 
            (source[source_count +1] & 0xC0) == 0x80)
        {
          // 110xxxxx
          c &= 0x1F;
          iterations = 1;
        }
      }
      else if ((c & 0xFF) >= 0xE0 && (c & 0xFF) <= 0xE7)
      {
        if (source_count < size -2 && 
            (source[source_count +1] & 0xC0) == 0x80 &&
            (source[source_count +2] & 0xC0) == 0x80)
        {
          // 1110xxxx
          c &= 0x0F;
          iterations = 2;
        }
      }
      else if ((c & 0xFF) >= 0xF0 && (c & 0xFF) <= 0xF4)
      {
        if (source_count < size -3 && 
            (source[source_count +1] & 0xC0) == 0x80 &&
            (source[source_count +2] & 0xC0) == 0x80 &&
            (source[source_count +3] & 0xC0) == 0x80)
        {
          // 11110xxx
          c &= 0x07;
          iterations = 3;
        }
      }

      if (iterations > 0)
      {
        int i;

        for (i = 0; i < iterations; i++)
        {
          c = (c << 6) | (source[++source_count] -0x80);
          if (log_charconv)
            sprintf(strchr(sourcechars, 0), "%02X", (unsigned char)source[source_count]);
        }

        // Euro character is 20AC in UTF-8, but A4 in ISO-8859-15:
        if ((c & 0xFF) == 0xAC)
          c = 0xA4;

        found = char2gsm((char)c, &newch);
        if (found == 2)
        {
          if (dest_count >= max -2)
            break;
          destination[dest_count++] = 0x1B;
        }
        if (found >= 1)
        {
          destination[dest_count++] = newch;
          if (log_charconv)
          {
            sprintf(logtmp, "%s(%02X[%c])->%s%02X", sourcechars, (unsigned char)c, prch(c),
                    (found == 2)? "Esc-" : "", (unsigned char)newch);
            if (gsm2char(newch, &tmpch, found))
              sprintf(strchr(logtmp, 0), "[%c]", tmpch);
            logch("%s ", logtmp);
          }
        }
        else
        {
          found = special_char2gsm((char)c, &newch);
          if (found)
          {
            destination[dest_count++] = newch;
            if (log_charconv)
            {
              sprintf(logtmp, "%s(%02X[%c])~>%02X", sourcechars, (unsigned char)c, prch(c), (unsigned char)newch);
              if (gsm2char(newch, &tmpch, 1))
                sprintf(strchr(logtmp, 0), "[%c]", tmpch);
              logch("%s ", logtmp);
            }
          }
          else
            source_count = saved_source_count;
        }
      }
    }

    // 3.1beta7: Try additional table:
    if (found == 0)
    {
      found = special_char2gsm(source[source_count], &newch);
      if (found)
      {
        destination[dest_count++] = newch;
        if (log_charconv)
        {
          sprintf(logtmp, "%02X[%c]~>%02X", (unsigned char)source[source_count], prch(source[source_count]), (unsigned char)newch);
          if (gsm2char(newch, &tmpch, 1))
            sprintf(strchr(logtmp, 0), "[%c]", tmpch);
          logch("%s ", logtmp);
        }
      }
    }

    if (found==0)
    {
      writelogfile0(LOG_NOTICE, 0,
        tb_sprintf("Cannot convert %i. character %c 0x%2X to GSM, you might need to update the translation tables.",
        source_count +1, source[source_count], source[source_count]));
#ifdef DEBUGMSG
  printf("%s\n", tb);
#endif
    }

    source_count++;
  }

  if (log_charconv)
    logch(NULL);

  // Terminate destination string with 0, however 0x00 are also allowed within the string.
  destination[dest_count]=0;
  return dest_count;
}

// Outputs to the file. Return value: 0 = ok, -1 = error.
int iso2utf8_file(FILE *fp, char *ascii, int userdatalength)
{
  int result = 0;
  int idx;
  unsigned int c;
  char tmp[10];
  int len;
  char logtmp[51];
  int i;

  if (!fp || userdatalength < 0)
    return -1;

#ifdef DEBUGMSG
  log_charconv = 1;
#endif

  if (log_charconv)
  {
    *logch_buffer = 0;
    logch("!! iso2utf8_file(..., userdatalength=%i)", userdatalength);
    logch(NULL);
  }

  for (idx = 0; idx < userdatalength; idx++)
  {
    len = 0;
    c = ascii[idx] & 0xFF;
    // Euro character is 20AC in UTF-8, but A4 in ISO-8859-15:
    if (c == 0xA4)
      c = 0x20AC;

    if (c <= 0x7F)
      tmp[len++] = (char)c;
    else if (c <= 0x7FF)
    {
      tmp[len++] = (char)( 0xC0 | ((c >> 6) & 0x1F) );
      tmp[len++] = (char)( 0x80 | (c & 0x3F) );
    }
    else if (c <= 0x7FFF) // or <= 0xFFFF ?
    {
      tmp[len++] = (char)( 0xE0 | ((c >> 12) & 0x0F) );
      tmp[len++] = (char)( 0x80 | ((c >> 6) & 0x3F) );
      tmp[len++] = (char)( 0x80 | (c & 0x3F) );
    }

    if (len == 0)
    {
      if (log_charconv)
        logch(NULL);
      writelogfile0(LOG_NOTICE, 0,
        tb_sprintf("UTF-8 conversion error with %i. ch 0x%2X %c.", idx +1, c, (char)c));
#ifdef DEBUGMSG
  printf("%s\n", tb);
#endif
    }
    else
    {
      if (log_charconv)
      {
        sprintf(logtmp, "%02X[%c]", (unsigned char)ascii[idx], prch(ascii[idx]));
        if (len > 1 || ascii[idx] != tmp[0])
        {
          strcat(logtmp, "->");
          for (i = 0; i < len; i++)
            sprintf(strchr(logtmp, 0), "%02X", (unsigned char)tmp[i]);
        }
        logch("%s ", logtmp);
      }

      if (fwrite(tmp, 1, len, fp) != (size_t)len)
      {
        if (log_charconv)
          logch(NULL);
        writelogfile0(LOG_NOTICE, 0, tb_sprintf("Fatal file write error in UTF-8 conversion"));
#ifdef DEBUGMSG
  printf("%s\n", tb);
#endif
        result = -1;
        break;
      }
    }
  }

  if (log_charconv)
    logch(NULL);
  return result;
}

int gsm2iso(char* source, int size, char* destination, int max)
{
  int source_count=0;
  int dest_count=0;
  char newch;

  if (source==0 || size==0)
  {
    destination[0]=0;
    return 0;
  }

  // Convert each character untl end of string
  while (source_count<size && dest_count<max)
  {
    if (source[source_count]!=0x1B)
    {  
      // search in normal translation table
      if (gsm2char(source[source_count], &newch, 1))
        destination[dest_count++] = newch;
      else if (source[source_count] == 0x24)
        destination[dest_count++] = (char)GSM_CURRENCY_SYMBOL_TO_ISO;
      else
      {
        writelogfile0(LOG_NOTICE, 0,
          tb_sprintf("Cannot convert GSM character 0x%2X to ISO, you might need to update the 1st translation table.",
          source[source_count]));
#ifdef DEBUGMSG
  printf("%s\n", tb);
#endif
      }
    }
    else if (++source_count<size)
    {
      // search in extended translation table
      if (gsm2char(source[source_count], &newch, 2))
        destination[dest_count++] = newch;
      else
      {
        writelogfile0(LOG_NOTICE, 0,
          tb_sprintf("Cannot convert extended GSM character 0x1B 0x%2X, you might need to update the 2nd translation table.",
          source[source_count]));
#ifdef DEBUGMSG
  printf("%s\n", tb);
#endif
      }
    }
    source_count++;
  }
  // Terminate destination string with 0, however 0x00 are also allowed within the string.
  destination[dest_count]=0;
  return dest_count;
}

#ifndef USE_ICONV
int decode_ucs2(char *buffer, int len)
{
  int i;
  char *d = buffer;
  int j;

  for (i = 0; i < len; )
  {
    switch (wctomb(d +i, (*(buffer +i) << 8 | *(buffer +i +1))))
    {
      case 2:
        i += 2;
        break;

      default:
        *(d +i) = *(buffer +i +1);
        d--;
        i += 2;
        break;
    }
  }
  i = (d -buffer) +len;
  *(buffer +i) = '\0';

  // 3.1.6: Fix euro character(s):
  for (j = 0; buffer[j]; j++)
    if (buffer[j] == (char) 0xAC)
      buffer[j] = (char) 0xA4;

  return i;
}
#endif

// ******************************************************************************
// Collect character conversion log, flush it if called with format==NULL.
// Also prints to the stdout, if debugging.
void logch(char* format, ...)
{
  va_list argp;
  char text[2048];
  int flush = 0;

  if (format)
  {
    va_start(argp, format);
    vsnprintf(text, sizeof(text), format, argp);
    va_end(argp);

    if (strlen(logch_buffer) +strlen(text) < sizeof(logch_buffer))
    {
      sprintf(strchr(logch_buffer, 0), "%s", text);
      // Line wrap after space character:
      // Outgoing conversion:
      if (strlen(text) >= 3)
        if (strcmp(text +strlen(text) -3, "20 ") == 0)
          flush = 1;
      // Incoming conversion:
      if (!flush)
        if (strlen(text) >= 6)
          if (strcmp(text +strlen(text) -6, "20[ ] ") == 0)
            flush = 1;
      // Line wrap after a reasonable length reached:
      if (!flush)
        if (strlen(logch_buffer) > 80)
          flush = 1;
    }
#ifdef DEBUGMSG
  printf("%s", text);
#endif
  }
  else
    flush = 1;

  if (flush)
  {
    if (*logch_buffer)
      writelogfile(LOG_DEBUG, 0, "charconv: %s", logch_buffer);
    *logch_buffer = 0;
#ifdef DEBUGMSG
  printf("\n");
#endif
  }
}

char prch(char ch)
{
  if ((unsigned char)ch >= ' ')
    return ch;
  return '.';
}

//hdr --------------------------------------------------------------------------------
int iso2utf8(
	//
	// Converts to the buffer. Returns -1 in case of error, >= 0 = length of dest.
	//
	char *ascii,
	int userdatalength,
	size_t ascii_size
)
{
	int result = 0;
	int idx;
	unsigned int c;
	char tmp[10];
	int len;
	char logtmp[51];
	int i;
	char *buffer;

	if (userdatalength < 0)
		return -1;

	if (!(buffer = (char *) malloc(ascii_size)))
		return -1;

#ifdef DEBUGMSG
	log_charconv = 1;
#endif

	if (log_charconv)
	{
		*logch_buffer = 0;
		logch("!! iso2utf8(..., userdatalength=%i)", userdatalength);
		logch(NULL);
	}

	for (idx = 0; idx < userdatalength; idx++)
	{
		len = 0;
		c = ascii[idx] & 0xFF;
		// Euro character is 20AC in UTF-8, but A4 in ISO-8859-15:
		if (c == 0xA4)
			c = 0x20AC;

		if (c <= 0x7F)
			tmp[len++] = (char) c;
		else if (c <= 0x7FF)
		{
			tmp[len++] = (char) (0xC0 | ((c >> 6) & 0x1F));
			tmp[len++] = (char) (0x80 | (c & 0x3F));
		}
		else if (c <= 0x7FFF)	// or <= 0xFFFF ?
		{
			tmp[len++] = (char) (0xE0 | ((c >> 12) & 0x0F));
			tmp[len++] = (char) (0x80 | ((c >> 6) & 0x3F));
			tmp[len++] = (char) (0x80 | (c & 0x3F));
		}

		if (len == 0)
		{
			if (log_charconv)
				logch(NULL);
			writelogfile0(LOG_NOTICE, 0, tb_sprintf("UTF-8 conversion error with %i. ch 0x%2X %c.", idx + 1, c, (char) c));
#ifdef DEBUGMSG
			printf("%s\n", tb);
#endif
		}
		else
		{
			if (log_charconv)
			{
				sprintf(logtmp, "%02X[%c]", (unsigned char) ascii[idx], prch(ascii[idx]));
				if (len > 1 || ascii[idx] != tmp[0])
				{
					strcat(logtmp, "->");
					for (i = 0; i < len; i++)
						sprintf(strchr(logtmp, 0), "%02X", (unsigned char) tmp[i]);
				}
				logch("%s ", logtmp);
			}

			if ((size_t) (result + len) < ascii_size - 1)
			{
				strncpy(buffer + result, tmp, len);
				result += len;
			}
			else
			{
				if (log_charconv)
					logch(NULL);
				writelogfile0(LOG_NOTICE, 0, tb_sprintf("Fatal error (buffer too small) in UTF-8 conversion"));
#ifdef DEBUGMSG
				printf("%s\n", tb);
#endif
				result = -1;
				break;
			}
		}
	}

	if (log_charconv)
		logch(NULL);

	if (result >= 0)
	{
		memcpy(ascii, buffer, result);
		ascii[result] = 0;
	}

	free(buffer);

	return result;
}

//hdr --------------------------------------------------------------------------------
int encode_7bit_packed(
	//
	// Encodes a string to GSM 7bit (USSD) packed format.
	// Returns number of septets.
	// Handles padding as defined on GSM 03.38 version 5.6.1 (ETS 300 900) page 17.
	//
	char *text,
	char *dest,
	size_t size_dest
)
{
	int len;
	int i;
	char buffer[512];
	char buffer2[512];
	char padding = '\r';

	//len = iso_utf8_2gsm(text, strlen(text), buffer2, sizeof(buffer2), ALPHABET_AUTO, 0);
	len = iso_utf8_2gsm(text, strlen(text), buffer2, sizeof(buffer2));

#ifdef DEBUGMSG
	printf("characters: %i\n", len);
	printf("characters %% 8: %i\n", len % 8);
#endif

	if ((len % 8 == 7) || (len % 8 == 0 && len && buffer2[len - 1] == padding))
	{
		if ((size_t) len < sizeof(buffer2) - 1)
		{
			buffer2[len++] = padding;
#ifdef DEBUGMSG
			printf("adding padding, characters: %i\n", len);
#endif
		}
	}

	i = text2pdu(buffer2, len, buffer, 0);
	snprintf(dest, size_dest, "%s", buffer);

#ifdef DEBUGMSG
	printf("octets: %i\n", strlen(buffer) / 2);
	for (len = 0; buffer[len]; len += 2)
		printf("%.2s ", buffer + len);
	printf("\n");
#endif
	return i;
}

//hdr --------------------------------------------------------------------------------
int decode_7bit_packed(
	//
	// Decodes GSM 7bit (USSD) packed string.
	// Returns length of dest. -1 in the case or error and "ERROR" in dest.
	// Handles padding as defined on GSM 03.38 version 5.6.1 (ETS 300 900) page 17.
	//
	char *text,
	char *dest,
	size_t size_dest
)
{
	int len;
	int i;
	char buffer[512];
	char buffer2[512];
	char *p;
	int septets;
	int padding = '\r';

	snprintf(buffer, sizeof(buffer), "%s", text);
	while ((p = strchr(buffer, ' ')))
		strcpyo(p, p + 1);
	for (i = 0; buffer[i]; i++)
		buffer[i] = toupper((int) buffer[i]);

	i = strlen(buffer);
	if (i % 2)
	{
		snprintf(dest, size_dest, "ERROR");
		return -1;
	}

	septets = i / 2 * 8 / 7;
	snprintf(buffer2, sizeof(buffer2), "%02X%s", septets, buffer);

#ifdef DEBUGMSG
	printf("septets: %i (0x%02X)\n", septets, septets);
	printf("septets %% 8: %i\n", septets % 8);
	printf("%s\n", buffer2);
#endif
	memset(buffer, 0, sizeof(buffer));
	pdu2text(buffer2, buffer, &len, 0, 0, 0, 0, 0);

	if ((septets % 8 == 0 && len && buffer[len - 1] == padding) || (septets % 8 == 1 && len > 1 && buffer[len - 1] == padding && buffer[len - 2] == padding))
	{
		len--;
#ifdef DEBUGMSG
		printf("removing padding, characters: %i\n", len);
#endif
	}

	i = gsm2iso(buffer, len, buffer2, sizeof(buffer2));
	if (incoming_utf8)
		i = iso2utf8(buffer2, i, sizeof(buffer2));
	snprintf(dest, size_dest, "%s", buffer2);

	return i;
}

// ******************************************************************************

#ifdef USE_ICONV
int iconv_init(void)
{
  // do noy use 'UTF8' alias - it not supported in cygwin/mingw
  return    (iconv4ucs = iconv_open("UTF-8", "UCS-2BE")) != (iconv_t)-1
         && (iconv2ucs = iconv_open("UCS-2BE", "UTF-8")) != (iconv_t)-1;
}

static size_t iconv_convert(iconv_t cd, char *buf, size_t* ilen, size_t maxlen)
{
  char tmp[MAXTEXT], *out, *in;
  size_t olen, rc;
  const char* err;

  if (!maxlen || !ilen || !*ilen || !buf)
    return 0;

  // reset conversion descriptor
  iconv(cd, NULL, NULL, NULL, NULL);
  err = NULL;
  in = buf;
  out = tmp;
  olen = sizeof(tmp);
  rc = iconv(cd, &in, ilen, &out, &olen);
  if (rc == (size_t)-1)
  {
    switch (errno)
    {
      case E2BIG:
        err = "Buffer to small";
        break;
      case EILSEQ:
        err = "Invalid sequnce";
        break;
      case EINVAL:
        err = "Incomplete sequence";
        break;
      default:
        err = strerror(errno);
        break;
    }
  }
  olen = sizeof(tmp) - olen;
  if (olen >= maxlen)
  {
    err = "output buffer too small";
    olen = maxlen;
  }
  memcpy(buf, tmp, olen);
  if (err != NULL)
    writelogfile(LOG_NOTICE, 0, "Unicode conversion error: %s", err);
  return olen;
}

size_t iconv_utf2ucs(char *buf, size_t len, size_t maxlen)
{
  return iconv_convert(iconv2ucs, buf, &len, maxlen);
}

size_t iconv_ucs2utf(char *buf, size_t len, size_t maxlen)
{
  return iconv_convert(iconv4ucs, buf, &len, maxlen);
}

size_t iconv_ucs2utf_chk(char *buf, size_t len, size_t maxlen)
{
  size_t olen, ilen = len;
  olen = iconv_convert(iconv4ucs, buf, &ilen, maxlen);
  buf[olen] = 0;
  if (ilen != 0)
    ilen = (len - ilen + 1) / 2;
  return ilen;
}

int is_ascii_gsm(char* buf, size_t len)
{
  char tmp;
  size_t i;
  for (i = 0; i < len; i++)
    if (buf[i] < ' ' || char2gsm(buf[i], &tmp) != 1)
      return 0;
  return 1;
}
#endif // USE_ICONV
