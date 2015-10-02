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

#ifndef PDU_H
#define PDU_H

#define SIZE_WARNING_HEADERS 4096

//Alphabet values: -1=GSM 0=ISO 1=binary 2=UCS2

#define NF_UNKNOWN 129
#define NF_INTERNATIONAL 145
#define NF_NATIONAL 161

int set_numberformat(int *numberformat, char *number, int number_type);

// Make the PDU string from a mesage text and destination phone number.
// The destination variable pdu has to be big enough. 
// alphabet indicates the character set of the message.
// flash_sms enables the flash flag.
// mode select the pdu version (old or new).
// if udh is true, then udh_data contains the optional user data header in hex dump, example: "05 00 03 AF 02 01"
void make_pdu(char* number, char* message, int messagelen, int alphabet, int flash_sms, int report, int udh,
              char* udh_data, char* mode, char* pdu, int validity, int replace_msg, int system_msg, int number_type, char *smsc);

// Splits a PDU string into the parts 
// Input: 
// pdu is the pdu string
// mode can be old or new and selects the pdu version
// Output:
// alphabet indicates the character set of the message.
// sendr Sender
// date and time Date/Time-stamp
// message is the message text or binary message
// smsc that sent this message
// with_udh returns the udh flag of the message
// is_statusreport is 1 if this was a status report
// is_unsupported_pdu is 1 if this pdu was not supported
// udh return the udh as hex dump
// Returns the length of the message 
int splitpdu(char *pdu, char *mode, int *alphabet, char *sendr, char *date, char *time, char *message,
             char *smsc, int *with_udh, char *a_udh_data, char *a_udh_type, int *is_statusreport,
             int *is_unsupported_pdu, char *from_toa, int *report, int *replace, char *warning_headers,
             int *flash, int bin_udh);

int octet2bin(char* octet);
int octet2bin_check(char* octet);
int isXdigit(char ch);

// Returns a length of udh (including UDHL), -1 if error.
// pdu is 0-terminated ascii(hex) pdu string with
// or without spaces.
int explain_udh(char *udh_type, char *pdu);

// Return value: -1 = error, 0 = not found.
// 1 = found 8bit, 2 = found 16bit.
// udh must be in header format, "05 00 03 02 03 02 "
int get_remove_concatenation(char *udh, int *message_id, int *parts, int *part);
int get_concatenation(char *udh, int *message_id, int *parts, int *part);
int remove_concatenation(char *udh);

int explain_toa(char *dest, char *octet_char, int octet_int);

void explain_status(char *dest, size_t size_dest, int status);

int get_pdu_details(char *dest, size_t size_dest, char *pdu, int mnumber);
void sort_pdu_details(char *dest);

int pdu2text(char *pdu, char *text, int *text_length, int *expected_length,
             int with_udh, char *udh, char *udh_type, int *errorpos);
int text2pdu(char* text, int length, char* pdu, char* udh);

#endif
