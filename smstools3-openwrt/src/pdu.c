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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include "pdu.h"
#include "smsd_cfg.h"
#include "logging.h"
#include "charset.h" // required for conversions of partial text content.
#include "extras.h"

#define MAX_ADDRESS_LENGTH 50
#define MAX_SMSC_ADDRESS_LENGTH 30

char *err_too_short = "string is too short";
char *err_pdu_content = "invalid character(s) in string";

int add_warning(char *buffer, char *format, ...)
{
  int result = 1;
  va_list argp;
  char text[2048];
  char *title = "Warning: ";

  va_start(argp, format);
  vsnprintf(text, sizeof(text), format, argp);
  va_end(argp);

  if (buffer)
  {
    if (strlen(buffer) + strlen(text) +strlen(title) +1/* for \n */ < SIZE_WARNING_HEADERS)
      sprintf(strchr(buffer, 0), "%s%s\n", title, text);
    else
    {
      result = 0;
      writelogfile(LOG_ERR, 1, "PDU %s%s", title, text);
    }
  }

  return result;
}

void pdu_error(char **err_str, char *title, int position, int length, char *format, ...)
{
  va_list argp;
  char text[2048];
  char *default_title = "PDU ERROR: ";
  char *used_title;
  char tmp[51];

  va_start(argp, format);
  vsnprintf(text, sizeof(text), format, argp);
  va_end(argp);

  used_title = (title)? title : default_title;

  if (position >= 0)
  {
    if (length > 0)
      sprintf(tmp, "Position %i,%i: ", position +1, length);
    else
      sprintf(tmp, "Position %i: ", position +1);
  }
  else
    *tmp = 0;

  if (!(*err_str))
  {
    if ((*err_str = (char *)malloc(strlen(tmp) +strlen(text) +strlen(used_title) +2)))
      *err_str[0] = 0;
  }
  else
    *err_str = (char *)realloc((void *)*err_str, strlen(*err_str) +strlen(used_title) +strlen(tmp) +strlen(text) +2);

  if (*err_str)
    sprintf(strchr(*err_str, 0), "%s%s%s\n", used_title, tmp, text);
}

int isXdigit(char ch)
{
  if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))
    return 1;
  return 0;
}

/* Swap every second character */
void swapchars(char* string) 
{
  int Length;
  int position;
  char c;

  Length=strlen(string);
  for (position=0; position<Length-1; position+=2)
  {
    c=string[position];
    string[position]=string[position+1];
    string[position+1]=c;
  }
}

// Converts an ascii text to a pdu string 
// text might contain zero values because this is a valid character code in sms
// character set. Therefore we need the length parameter.
// If udh is not 0, then insert it to the beginning of the message.
// The string must be in hex-dump format: "05 00 03 AF 02 01". 
// The first byte is the size of the UDH.
int text2pdu(char* text, int length, char* pdu, char* udh)
{
  char tmp[500];
  char octett[10];
  int pdubitposition;
  int pdubyteposition=0;
  int character;
  int bit;
  int pdubitnr;
  int counted_characters=0;
  int udh_size_octets;   // size of the udh in octets, should equal to the first byte + 1
  int udh_size_septets;  // size of udh in septets (7bit text characters)
  int fillbits;          // number of filler bits between udh and ud.
  int counter;

#ifdef DEBUGMSG
  printf("!! text2pdu(text=..., length=%i, pdu=..., udh=%s\n",length,udh);
#endif
  pdu[0]=0;
  // Check the udh
  if (udh)
  {
    udh_size_octets=(strlen(udh)+2)/3;
    udh_size_septets=((udh_size_octets)*8+6)/7;
    fillbits=7-(udh_size_octets % 7);
    if (fillbits==7)
      fillbits=0;

    // copy udh to pdu, skipping the space characters
    for (counter=0; counter<udh_size_octets; counter++)
    {
      pdu[counter*2]=udh[counter*3];
      pdu[counter*2+1]=udh[counter*3+1];
    }
    pdu[counter*2]=0;
#ifdef DEBUGMSG
  printf("!! pdu=%s, fillbits=%i\n",pdu,fillbits);
#endif
  } 
  else
  {
    udh_size_octets=0;
    udh_size_septets=0; 
    fillbits=0;
  }
  // limit size of the message to maximum allowed size
  if (length>maxsms_pdu-udh_size_septets)
    length=maxsms_pdu-udh_size_septets;
  //clear the tmp buffer
  for (character=0;(size_t)character<sizeof(tmp);character++)
    tmp[character]=0;
  // Convert 8bit text stream to 7bit stream
  for (character=0;character<length;character++)
  {
    counted_characters++;
    for (bit=0;bit<7;bit++)
    {
      pdubitnr=7*character+bit+fillbits;
      pdubyteposition=pdubitnr/8;
      pdubitposition=pdubitnr%8;
      if (text[character] & (1<<bit))
        tmp[pdubyteposition]=tmp[pdubyteposition] | (1<<pdubitposition);
      else
        tmp[pdubyteposition]=tmp[pdubyteposition] & ~(1<<pdubitposition);
    }
  }
  tmp[pdubyteposition+1]=0;
  // convert 7bit stream to hex-dump
  for (character=0;character<=pdubyteposition; character++)
  {
    sprintf(octett,"%02X",(unsigned char) tmp[character]);
    strcat(pdu,octett);
  }
#ifdef DEBUGMSG
  printf("!! pdu=%s\n",pdu);
#endif
  return counted_characters+udh_size_septets;
}

/* Converts binary to PDU string, this is basically a hex dump. */
void binary2pdu(char* binary, int length, char* pdu)
{
  int character;
  char octett[10];

  if (length>maxsms_binary)
    length=maxsms_binary;
  pdu[0]=0;
  for (character=0;character<length; character++)
  {
    sprintf(octett,"%02X",(unsigned char) binary[character]);
    strcat(pdu,octett);
  }
}

int set_numberformat(int *numberformat, char *number, int number_type)
{
  int nf = NF_INTERNATIONAL;
  char *p;

  if (numberformat)
    nf = *numberformat;

  if (nf == NF_INTERNATIONAL)
  {
    // If international prefixes are defined, international format is used if a number matches to the list.
    // If it does not match, national format will be used.
    if (*international_prefixes)
    {
      p = international_prefixes;
      do
      {
        if (!strncmp(number, p, strlen(p)))
          break;
        p += strlen(p) +1;
      }
      while (*p);

      if (!(*p))
        nf = NF_NATIONAL;
    }
    
    if (nf == NF_INTERNATIONAL)
    {
      // If national prefixes are defined, national format is used if a number matches to the list.
      if (*national_prefixes)
      {
        p = national_prefixes;
        do
        {
          if (!strncmp(number, p, strlen(p)))
          {
            nf = NF_NATIONAL;
            break;
          }
          p += strlen(p) +1;
        }
        while (*p);
      }
    }
  }

  // Finally, user setting overrides all...
  switch (number_type)
  {
    case 0: // "unknown"
      nf = NF_UNKNOWN; // 1000 0001
      break;

    case 1: // "international"
      nf = NF_INTERNATIONAL; // 1001 0001
      break;

    case 2: // "national"
      nf = NF_NATIONAL; // 1010 0001
      break;
  }

  if (numberformat)
    *numberformat = nf;

  return nf;
}

// Make the PDU string from a mesage text and destination phone number.
// The destination variable pdu has to be big enough. 
// alphabet indicates the character set of the message.
// flash_sms enables the flash flag.
// mode select the pdu version (old or new).
// if udh is true, then udh_data contains the optional user data header in hex dump, example: "05 00 03 AF 02 01"

void make_pdu(char* number, char* message, int messagelen, int alphabet, int flash_sms, int report, int with_udh,
              char* udh_data, char* mode, char* pdu, int validity, int replace_msg, int system_msg, int number_type, char *smsc)
{
  int coding;
  int flags;
  char tmp[SIZE_TO];
  char tmp2[500];
  int numberformat;
  int numberlength;
  char *p;
  int l;
  char tmp_smsc[SIZE_SMSC];

  if (number[0] == 's')  // Is number starts with s, then send it without number format indicator
  {
    numberformat = NF_UNKNOWN;
    snprintf(tmp, sizeof(tmp), "%s", number +1);
  }
  else
  {
    numberformat = NF_INTERNATIONAL;
    snprintf(tmp, sizeof(tmp), "%s", number);
  }

  set_numberformat(&numberformat, tmp, number_type);

  numberlength=strlen(tmp);
  // terminate the number with F if the length is odd
  if (numberlength%2)
    strcat(tmp,"F");
  // Swap every second character
  swapchars(tmp);

  // 3.1.12:
  *tmp_smsc = 0;
  if (DEVICE.smsc_pdu && (smsc[0] || DEVICE.smsc[0]))
  {
    p = (smsc[0]) ? smsc : DEVICE.smsc;
    while (*p == '+')
      p++;
    snprintf(tmp_smsc, sizeof(tmp_smsc), "%s", p);
    if (strlen(tmp_smsc) % 2 && strlen(tmp_smsc) < sizeof(tmp_smsc) - 1)
      strcat(tmp_smsc, "F");
    swapchars(tmp_smsc);
  }

  flags=1; // SMS-Sumbit MS to SMSC
  if (with_udh)
    flags+=64; // User Data Header
  if (strcmp(mode,"old")!=0)
    flags+=16; // Validity field
  if (report>0)
    flags+=32; // Request Status Report

  if (alphabet == 1)
    coding = 4; // 8bit binary
  else if (alphabet == 2)
    coding = 8; // 16bit
  else
    coding = 0; // 7bit
  if (flash_sms > 0)
    coding += 0x10; // Bits 1 and 0 have a message class meaning (class 0, alert)

  /* Create the PDU string of the message */
  if (alphabet==1 || alphabet==2 || system_msg)
  {
    // Unicode message can be concatenated:
    //if (alphabet == 2 && with_udh)
    // Binary message can be concatenated too:
    if (with_udh)
    {
      strcpy(tmp2, udh_data);
      while ((p = strchr(tmp2, ' ')))
        strcpyo(p, p +1);
      l = strlen(tmp2) /2;
      binary2pdu(message, messagelen, strchr(tmp2, 0));
      messagelen += l;
    }
    else
      binary2pdu(message,messagelen,tmp2);
  }
  else
    messagelen=text2pdu(message,messagelen,tmp2,udh_data);

  /* concatenate the first part of the PDU string */
  if (strcmp(mode,"old")==0)
    sprintf(pdu,"%02X00%02X%02X%s00%02X%02X",flags,numberlength,numberformat,tmp,coding,messagelen);
  else
  {
    int proto = 0;

    if (validity < 0 || validity > 255)
      validity = validity_period;

    if (system_msg)
    {
      proto = 0x40;
      coding = 0xF4;  // binary

      // 3.1.7:
      if (system_msg == 2)
      {
        proto += (0x7F - 0x40); // SS (no show)
        coding += 2;    // store to sim
      }
    }
    else if (replace_msg >= 1 && replace_msg <= 7)
       proto = 0x40 + replace_msg;

    //sprintf(pdu, "00%02X00%02X%02X%s%02X%02X%02X%02X", flags, numberlength, numberformat, tmp,
    // (system_msg) ? 0x40 : (replace_msg >= 1 && replace_msg <= 7) ? 0x40 + replace_msg : 0,
    // (system_msg) ? 0xF4 : coding, validity, messagelen);

    // 3.1.12:
    //sprintf(pdu, "00%02X00%02X%02X%s%02X%02X%02X%02X", flags, numberlength, numberformat, tmp, proto, coding, validity, messagelen);
    if (*tmp_smsc)
      sprintf(pdu, "%02X%s%s", (int)strlen(tmp_smsc) / 2 + 1, (tmp_smsc[1] == '0')? "81": "91", tmp_smsc);
    else
      strcpy(pdu, "00");
    sprintf(strchr(pdu, 0), "%02X00%02X%02X%s%02X%02X%02X%02X", flags, numberlength, numberformat, tmp, proto, coding, validity, messagelen);
  }

  /* concatenate the text to the PDU string */
  strcat(pdu,tmp2);
}

int octet2bin(char* octet) /* converts an octet to a 8-Bit value */
{
  int result=0;

  if (octet[0]>57)
    result+=octet[0]-55;
  else
    result+=octet[0]-48;
  result=result<<4;
  if (octet[1]>57)
    result+=octet[1]-55;
  else
    result+=octet[1]-48;
  return result;
}

// Converts an octet to a 8bit value,
// returns < in case of error.
int octet2bin_check(char *octet)
{
  if (octet[0] == 0)
    return -1;
  if (octet[1] == 0)
    return -2;
  if (!isXdigit(octet[0]))
    return -3;
  if (!isXdigit(octet[1]))
    return -4;
  return octet2bin(octet);
}

// Return value: -1 = error, 0 = not found.
// 1 = found 8bit, 2 = found 16bit.
// udh must be in header format, "05 00 03 02 03 02 "
int get_remove_concatenation(char *udh, int *message_id, int *parts, int *part)
{
  int udh_length;
  int octets;
  int idx;
  char *con_start = NULL;
  int id;
  int i;
  char tmp[10];

  if ((udh_length = octet2bin_check(udh)) < 0)
    return -1;

  octets = strlen(udh) /3;
  idx = 1;
  while (idx < octets)
  {
    if ((id = octet2bin_check(udh +idx *2 +idx)) < 0)
      return -1;

    if (id == 0x00 || id == 0x08)
    {
      // It's here.
      con_start = udh +idx *2 +idx;
      if (++idx >= octets)
        return -1;
      i = octet2bin_check(udh +idx *2 +idx);
      if ((id == 0x00 && i != 0x03) ||
          (id == 0x08 && i != 0x04))
        return -1;
      if (++idx >= octets)
        return -1;
      if ((*message_id = octet2bin_check(udh +idx *2 +idx)) < 0)
        return -1;
      if (id == 0x08)
      {
        if (++idx >= octets)
          return -1;
        if ((i = octet2bin_check(udh +idx *2 +idx)) < 0)
          return -1;
        *message_id = *message_id *0xFF +i;
      }
      if (++idx >= octets)
        return -1;
      if ((*parts = octet2bin_check(udh +idx *2 +idx)) < 0)
        return -1;
      if (++idx >= octets)
        return -1;
      if ((*part = octet2bin_check(udh +idx *2 +idx)) < 0)
        return -1;
      if (++idx >= octets)
        *con_start = 0;
      else
        strcpy(con_start, udh +idx *2 +idx);
      i = (id == 0x00)? 5 : 6;
      udh_length -= i;
      if (udh_length > 0)
      {
        sprintf(tmp, "%02X", udh_length);
        memcpy(udh, tmp, 2);
      }
      else
        *udh = 0;
      return (id == 0x00)? 1 : 2;
    }
    else
    {
      // Something else data. Get the length and skip.
      if (++idx >= octets)
        return -1;
      if ((i = octet2bin_check(udh +idx *2 +idx)) < 0)
        return -1;
      idx += i +1;
    }
  }

  return 0;
}

int get_concatenation(char *udh, int *message_id, int *parts, int *part)
{
  char *tmp;
  int result = -1;

  if ((tmp = strdup(udh)))
  {
    result = get_remove_concatenation(tmp, message_id, parts, part);
    free(tmp);
  }

  return result;
}

int remove_concatenation(char *udh)
{
  int message_id;
  int parts;
  int part;

  return get_remove_concatenation(udh, &message_id, &parts, &part);
}

// Returns a length of udh (including UDHL), -1 if error.
// pdu is 0-terminated ascii(hex) pdu string with
// or without spaces.
int explain_udh(char *udh_type, char *pdu)
{
  int udh_length;
  int idx;
  char *Src_Pointer;
  char *p;
  int i;
  char tmp[512];
  char buffer[1024];

  *udh_type = 0;
  if (strlen(pdu) >= sizeof(buffer))
    return -1;
  strcpy(buffer, pdu);
  while ((p = strchr(buffer, ' ')))
    strcpyo(p, p +1);

  if ((udh_length = octet2bin_check(buffer)) < 0)
    return -1;
  udh_length++;
  if ((size_t)(udh_length *2) > strlen(buffer))
    return -1;
  sprintf(udh_type, "Length=%i", udh_length);
  idx = 1;
  while (idx < udh_length)
  {
    Src_Pointer = buffer +idx *2;
    p = NULL;
    i = octet2bin_check(Src_Pointer);
    switch (i)
    {
      case -1:
        //sprintf(strchr(udh_type, 0), ", ERROR");
        return -1;

      // 3GPP TS 23.040 version 6.8.1 Release 6 - ETSI TS 123 040 V6.8.1 (2006-10)
      case 0x00: p = "Concatenated short messages, 8-bit reference number"; break;
      case 0x01: p = "Special SMS Message Indication"; break;
      case 0x02: p = "Reserved"; break;
      //case 0x03: p = "Value not used to avoid misinterpretation as <LF> character"; break;
      case 0x04: p = "Application port addressing scheme, 8 bit address"; break;
      case 0x05: p = "Application port addressing scheme, 16 bit address"; break;
      case 0x06: p = "SMSC Control Parameters"; break;
      case 0x07: p = "UDH Source Indicator"; break;
      case 0x08: p = "Concatenated short message, 16-bit reference number"; break;
      case 0x09: p = "Wireless Control Message Protocol"; break;
      case 0x0A: p = "Text Formatting"; break;
      case 0x0B: p = "Predefined Sound"; break;
      case 0x0C: p = "User Defined Sound (iMelody max 128 bytes)"; break;
      case 0x0D: p = "Predefined Animation"; break;
      case 0x0E: p = "Large Animation (16*16 times 4 = 32*4 =128 bytes)"; break;
      case 0x0F: p = "Small Animation (8*8 times 4 = 8*4 =32 bytes)"; break;
      case 0x10: p = "Large Picture (32*32 = 128 bytes)"; break;
      case 0x11: p = "Small Picture (16*16 = 32 bytes)"; break;
      case 0x12: p = "Variable Picture"; break;
      case 0x13: p = "User prompt indicator"; break;
      case 0x14: p = "Extended Object"; break;
      case 0x15: p = "Reused Extended Object"; break;
      case 0x16: p = "Compression Control"; break;
      case 0x17: p = "Object Distribution Indicator"; break;
      case 0x18: p = "Standard WVG object"; break;
      case 0x19: p = "Character Size WVG object"; break;
      case 0x1A: p = "Extended Object Data Request Command"; break;
      case 0x20: p = "RFC 822 E-Mail Header"; break;
      case 0x21: p = "Hyperlink format element"; break;
      case 0x22: p = "Reply Address Element"; break;
      case 0x23: p = "Enhanced Voice Mail Information"; break;
    }

    if (!p)
    {
      if (i >= 0x1B && i <= 0x1F)
        p = "Reserved for future EMS features";
      else if (i >= 0x24 && i <= 0x6F)
        p = "Reserved for future use";
      else if (i >= 0x70 && i <= 0x7F)
        p = "(U)SIM Toolkit Security Headers";
      else if (i >= 0x80 && i <= 0x9F)
        p = "SME to SME specific use";
      else if (i >= 0xA0 && i <= 0xBF)
        p = "Reserved for future use";
      else if (i >= 0xC0 && i <= 0xDF)
        p = "SC specific use";
      else if (i >= 0xE0 && i <= 0xFF)
        p = "Reserved for future use";
    }

    if (!p)
      p = "unknown";
    sprintf(tmp, ", [%.2s]%s", Src_Pointer, p);
    if (strlen(udh_type) + strlen(tmp) >= SIZE_UDH_TYPE)
      return -1;
    sprintf(strchr(udh_type, 0), "%s", tmp);

    // Next octet is length of data:
    if ((i = octet2bin_check(Src_Pointer +2)) < 0)
      return -1;
    if ((size_t)(i *2) > strlen(Src_Pointer +4))
      return -1;
    idx += i +2;
    if (idx > udh_length)
      return -1; // Incorrect UDL or length of Information Element.
  }

  return udh_length;
}

/* converts a PDU-String to text, text might contain zero values! */
/* the first octet is the length */
/* return the length of text, -1 if there is a PDU error, -2 if PDU is too short */
/* with_udh must be set already if the message has an UDH */
/* this function does not detect the existance of UDH automatically. */
int pdu2text(char *pdu, char *text, int *text_length, int *expected_length,
             int with_udh, char *udh, char *udh_type, int *errorpos) 
{
  int bitposition;
  int byteposition;
  int byteoffset;
  int charcounter;
  int bitcounter;
  int septets;
  int octets;
  int udhsize;
  int octetcounter;
  int skip_characters = 0;
  char c;
  char binary = 0;
  int i;
  int result;

#ifdef DEBUGMSG
  printf("!! pdu2text(pdu=%s,...)\n",pdu);
#endif

  if (udh) 
    *udh = 0;
  if (udh_type)
    *udh_type = 0;

  if ((septets = octet2bin_check(pdu)) < 0)
  {
    if (errorpos)
      *errorpos = -1 * septets -3;
    return (septets >= -2)? -2: -1;
  }

  if (with_udh)
  {
    // copy the data header to udh and convert to hex dump
    // There was at least one octet and next will give an error if there is no more data:
    if ((udhsize = octet2bin_check(pdu +2)) < 0)
    {
      if (errorpos)
        *errorpos = -1 * udhsize -3 +2;
      return (udhsize >= -2)? -2: -1;
    }

    i = 0;
    result = -1;
    for (octetcounter=0; octetcounter<udhsize+1; octetcounter++)
    {
      if (octetcounter *3 +3 >= SIZE_UDH_DATA)
      {
        i = octetcounter *2 +2;
        result = -2;
        break;
      }
      udh[octetcounter*3]=pdu[(octetcounter<<1)+2];
      if (!isXdigit(udh[octetcounter *3]))
      {
        i = octetcounter *2 +2;
        if (!udh[octetcounter *3])
          result = -2;
        break;
      }
      udh[octetcounter*3+1]=pdu[(octetcounter<<1)+3];
      if (!isXdigit(udh[octetcounter *3 +1]))
      {
        i = octetcounter *2 +3;
        if (!udh[octetcounter *3 +1])
          result = -2;
        break;
      }
      udh[octetcounter *3 +2] = ' ';
      udh[octetcounter *3 +3] = 0;
    }

    if (i)
    {
      if (errorpos)
        *errorpos = i;
      return result;
    }

    if (udh_type)
      if (explain_udh(udh_type, pdu +2) < 0)
        if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
          sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");

    // Calculate how many text charcters include the UDH.
    // After the UDH there may follow filling bits to reach a 7bit boundary.
    skip_characters=(((udhsize+1)*8)+6)/7;
#ifdef DEBUGMSG
  printf("!! septets=%i\n",septets);
  printf("!! udhsize=%i\n",udhsize);
  printf("!! skip_characters=%i\n",skip_characters);
#endif
  }

  if (expected_length)
    *expected_length = septets -skip_characters;

  // Convert from 8-Bit to 7-Bit encapsulated in 8 bit 
  // skipping storing of some characters used by UDH.
  // 3.1beta7: Simplified handling to allow partial decodings to be shown.
  octets = (septets *7 +7) /8;     
  bitposition = 0;
  octetcounter = 0;
  for (charcounter = 0; charcounter < septets; charcounter++)
  {
    c = 0;
    for (bitcounter = 0; bitcounter < 7; bitcounter++)
    {
      byteposition = bitposition /8;
      byteoffset = bitposition %8;
      while (byteposition >= octetcounter && octetcounter < octets)
      {
        if ((i = octet2bin_check(pdu +(octetcounter << 1) +2)) < 0)
        {
          if (errorpos)
          {
            *errorpos = octetcounter *2 +2;
            if (i == -2 || i == -4)
              (*errorpos)++;
          }
          if (text_length)
            *text_length = charcounter -skip_characters;
          return (i >= -2)? -2: -1;
        }
        binary = i;
        octetcounter++;
      }
      if (binary & (1 << byteoffset))
        c = c | 128;
      bitposition++;
      c = (c >> 1) & 127; // The shift fills with 1, but 0 is wanted.
    }
    if (charcounter >= skip_characters)
      text[charcounter -skip_characters] = c; 
  }

  if (text_length)
    *text_length = charcounter -skip_characters;

  if (charcounter -skip_characters >= 0)
    text[charcounter -skip_characters] = 0;
  return charcounter -skip_characters;
}

int pdu2text0(char *pdu, char *text)
{
  return pdu2text(pdu, text, 0, 0, 0, 0, 0, 0);
}

// Converts a PDU string to binary. Return -1 if there is a PDU error, -2 if PDU is too short.
// Version > 3.0.9, > 3.1beta6 handles also udh.
int pdu2binary(char* pdu, char* binary, int *data_length, int *expected_length,
               int with_udh, char *udh, char *udh_type, int *errorpos)
{
  int octets;
  int octetcounter;
  int i;
  int udhsize = 0;
  int skip_octets = 0;
  int result;

  *udh = 0;
  *udh_type = 0;

  if ((octets = octet2bin_check(pdu)) < 0)
  {
    *errorpos = -1 * octets -3;
    return (octets >= -2)? -2: -1;
  }

  if (with_udh)
  {
    // copy the data header to udh and convert to hex dump
    // There was at least one octet and next will give an error if there is no more data:
    if ((udhsize = octet2bin_check(pdu +2)) < 0)
    {
      *errorpos = -1 * udhsize -3 +2;
      return (udhsize >= -2)? -2: -1;
    }

    i = 0;
    result = -1;
    for (octetcounter = 0; octetcounter < udhsize +1; octetcounter++)
    {
      if (octetcounter *3 +3 >= SIZE_UDH_DATA)
      {
        i = octetcounter *2 +2;
        result = -2;
        break;
      }
      udh[octetcounter *3] = pdu[(octetcounter << 1) +2];
      if (!isXdigit(udh[octetcounter *3]))
      {
        i = octetcounter *2 +2;
        if (!udh[octetcounter *3])
          result = -2;
        break;
      }
      udh[octetcounter *3 +1] = pdu[(octetcounter << 1) +3];
      if (!isXdigit(udh[octetcounter *3 +1]))
      {
        i = octetcounter *2 +3;
        if (!udh[octetcounter *3 +1])
          result = -2;
        break;
      }
      udh[octetcounter *3 +2] = ' '; 
      udh[octetcounter *3 +3] = 0; 
    }

    if (i)
    {
      *errorpos = i;
      return result;
    }

    if (udh_type)
      if (explain_udh(udh_type, pdu +2) < 0)
        if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
          sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");

    skip_octets = udhsize +1;
  }

  *expected_length = octets -skip_octets;

  for (octetcounter = 0; octetcounter < octets -skip_octets; octetcounter++)
  {
    if ((i = octet2bin_check(pdu +(octetcounter << 1) +2 +(skip_octets *2))) < 0)
    {
      *errorpos = octetcounter *2 +2 +(skip_octets *2);
      if (i == -2 || i == -4)
        (*errorpos)++;
      *data_length = octetcounter;
      return (i >= -2)? -2: -1;
    }
    else
      binary[octetcounter] = i;
  }

  if (octets -skip_octets >= 0)
    binary[octets -skip_octets] = 0;
  *data_length = octets -skip_octets;
  return octets -skip_octets;
}

int explain_toa(char *dest, char *octet_char, int octet_int)
{
  int result;
  char *p = "reserved";

  if (octet_char)
    result = octet2bin_check(octet_char);
  else
    result = octet_int;

  if (result != -1)
  {
    switch ((result & 0x70) >> 4)
    {
      case 0: p = "unknown"; break;
      case 1: p = "international"; break;
      case 2: p = "national"; break;
      case 3: p = "network specific"; break;
      case 4: p = "subsciber"; break;
      case 5: p = "alphanumeric"; break;
      case 6: p = "abbreviated"; break;
      //case 7: p = "reserved"; break;
    }

    if (octet_char)
      sprintf(dest, "%.2s %s", octet_char, p);
    else
      sprintf(dest, "%02X %s", octet_int, p);

    switch (result & 0x0F)
    {
      case 0: p = "unknown"; break;
      case 1: p = "ISDN/telephone"; break;
      case 3: p = "data"; break;
      case 4: p = "telex"; break;
      case 8: p = "national"; break;
      case 9: p = "private"; break;
      case 10: p = "ERMES"; break;
      //default: p = "reserved"; break;
    }
    sprintf(strchr(dest, 0), ", %s", p);
  }

  return result;
}

// 3.1.14:
void explain_status(char *dest, size_t size_dest, int status)
{
  char *p = "unknown";

  switch (status)
  {
    case 0: p = "Ok,short message received by the SME"; break;
    case 1: p = "Ok,short message forwarded by the SC to the SME but the SC is unable to confirm delivery"; break;
    case 2: p = "Ok,short message replaced by the SC"; break;

    // Temporary error, SC still trying to transfer SM
    case 32: p = "Still trying,congestion"; break;
    case 33: p = "Still trying,SME busy"; break;
    case 34: p = "Still trying,no response sendr SME"; break;
    case 35: p = "Still trying,service rejected"; break;
    case 36: p = "Still trying,quality of service not available"; break;
    case 37: p = "Still trying,error in SME"; break;
    // 38...47: Reserved
    // 48...63: Values specific to each SC

    // Permanent error, SC is not making any more transfer attempts
    case 64: p = "Error,remote procedure error"; break;
    case 65: p = "Error,incompatible destination"; break;
    case 66: p = "Error,connection rejected by SME"; break;
    case 67: p = "Error,not obtainable"; break;
    case 68: p = "Error,quality of service not available"; break;
    case 69: p = "Error,no interworking available"; break;
    case 70: p = "Error,SM validity period expired"; break;
    case 71: p = "Error,SM deleted by originating SME"; break;
    case 72: p = "Error,SM deleted by SC administration"; break;
    case 73: p = "Error,SM does not exist"; break;
    // 74...79: Reserved
    // 80...95: Values specific to each SC

    // Permanent error, SC is not making any more transfer attempts
    case 96: p = "Error,congestion"; break;
    case 97: p = "Error,SME busy"; break;
    case 98: p = "Error,no response sendr SME"; break;
    case 99: p = "Error,service rejected"; break;
    case 100: p = "Error,quality of service not available"; break;
    case 101: p = "Error,error in SME"; break;
    // 102...105: Reserved
    // 106...111: Reserved
    // 112...127: Values specific to each SC
    // 128...255: Reserved

    default:
      if (status >= 48 && status <= 63)
        p = "Temporary error, SC specific, unknown";
      else if ((status >= 80 && status <= 95) || (status >= 112 && status <= 127))
        p = "Permanent error, SC specific, unknown";
  }

  snprintf(dest, size_dest, "%s", p);
}

// Subroutine for messages type 0 (SMS-Deliver)
// Input:
// Src_Pointer points to the PDU string
// Output:
// sendr Sender
// date and time Date/Time-stamp
// message the message text or binary data
// bin_udh: 1 if udh is taken from the PDU with binary messages too
// returns length of message
int split_type_0(char *full_pdu, char* Src_Pointer, int* alphabet, char* sendr, char* date, char* time,
                 char* message, int *message_length, int *expected_length, int with_udh, char* udh_data,
                 char *udh_type, char *from_toa, int *replace, char **err_str, char *warning_headers,
                 int *flash, int bin_udh)
{
  int Length;
  int padding = 0;
  char tmpsender[100];
  int result = 0;
  int i;
  int errorpos;

#ifdef DEBUGMSG
  printf("!! split_type_0(Src_Pointer=%s, ...\n",Src_Pointer);
#endif

  // There should be at least address-length and address-type:
  if (strlen(Src_Pointer) < 4)
    pdu_error(err_str, 0, Src_Pointer -full_pdu, 4, "While trying to read address length and address type: %s", err_too_short);
  else
  {
    Length = octet2bin_check(Src_Pointer);
    // 3.1.5: Sender address length can be zero. There is still address type octet (80).
    if (Length < 0 || Length > MAX_ADDRESS_LENGTH)
      pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid sender address length: \"%.2s\"", Src_Pointer);
    else
    if (Length == 0)
      Src_Pointer += 4;
    else
    {
      padding=Length%2;
      Src_Pointer+=2;
      i = explain_toa(from_toa, Src_Pointer, 0);
      if (i < 0)
        pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid sender address type: \"%.2s\"", Src_Pointer);
      else if (i < 0x80)
        pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Missing bit 7 in sender address type: \"%.2s\"", Src_Pointer);
      else
      {
        Src_Pointer += 2;
        if ((i & 112) == 80)
        {  // Sender is alphanumeric
          if (strlen(Src_Pointer) < (size_t)(Length +padding))
            pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                      "While trying to read sender address (alphanumeric, length %i): %s",
                      Length +padding, err_too_short);
          else
          {
            snprintf(tmpsender,Length+3+padding,"%02X%s",Length*4/7,Src_Pointer);
            if (pdu2text0(tmpsender, sendr) < 0)
              pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                        "While reading alphanumeric sender address: %s", err_pdu_content);
          }
        }
        else
        {  // sender is numeric
          if (strlen(Src_Pointer) < (size_t)(Length +padding))
            pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                      "While trying to read sender address (numeric, length %i): %s",
                      Length +padding, err_too_short);
          else
          {
            strncpy(sendr, Src_Pointer, Length +padding);
            sendr[Length +padding] = 0;
            swapchars(sendr);
            i = Length +padding -1;
            if (padding)
            {
              if (sendr[i] != 'F')
                add_warning(warning_headers, "Length of numeric sender address is odd, but not terminated with 'F'.");
              else
                sendr[i] = 0;
            }
            else
            {
              if (sendr[i] == 'F')
              {
                add_warning(warning_headers, "Length of numeric sender address is even, but still was terminated with 'F'.");
                sendr[i] = 0;
              }
            }

            for (i = 0; sendr[i]; i++)
              if (!isdigitc(sendr[i]))
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding, "Invalid character(s) in sender address: \"%s\"", sendr);
                // *sendr = 0;
                add_warning(warning_headers, "Invalid character(s) in sender address.");
                break;
              }
          }
        }
      }
    }

    if (!(*err_str))
    {
      Src_Pointer += Length +padding;
      // Next there should be:
      // XX protocol identifier
      // XX data encoding scheme
      // XXXXXXXXXXXXXX time stamp, 7 octets
      // XX length of user data
      // ( XX... user data  )
      if (strlen(Src_Pointer) < 20)
        pdu_error(err_str, 0, Src_Pointer -full_pdu, 20, "While trying to read TP-PID, TP-DSC, TP-SCTS and TP-UDL: %s", err_too_short);
      else
      {
        if ((i = octet2bin_check(Src_Pointer)) < 0)
          pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid protocol identifier: \"%.2s\"", Src_Pointer);
        else
        {
          if ((i & 0xF8) == 0x40)
            *replace = (i & 0x07);
          Src_Pointer += 2;
          if ((i = octet2bin_check(Src_Pointer)) < 0)
            pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid data encoding scheme: \"%.2s\"", Src_Pointer);
          else
          {
            *alphabet = (i & 0x0C) >>2;
            if (*alphabet == 3)
              pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid alphabet in data encoding scheme: value 3 is not supported.");
              // ...or should this be a warning? If so, GSM alphabet is then used as a default.

            if (*alphabet == 0)
              *alphabet = -1;

            // 3.1: Check if this message was a flash message:
            if (i & 0x10)
              if (!(i & 0x01))
                *flash = 1;

            if (!(*err_str))
            {
              Src_Pointer += 2;
              sprintf(date,"%c%c-%c%c-%c%c",Src_Pointer[1],Src_Pointer[0],Src_Pointer[3],Src_Pointer[2],Src_Pointer[5],Src_Pointer[4]);
              if (!isdigitc(date[0]) || !isdigitc(date[1]) || !isdigitc(date[3]) || !isdigitc(date[4]) || !isdigitc(date[6]) || !isdigitc(date[7]))
              {
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid character(s) in date of Service Centre Time Stamp: \"%s\"", date);
                *date = 0;
              }
              else if (atoi(date +3) > 12 || atoi(date +6) > 31)
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid value(s) in date of Service Centre Time Stamp: \"%s\"", date);
                // *date = 0;
                add_warning(warning_headers, "Invalid values(s) in date of Service Centre Time Stamp.");
              }

              Src_Pointer += 6;
              sprintf(time,"%c%c:%c%c:%c%c",Src_Pointer[1],Src_Pointer[0],Src_Pointer[3],Src_Pointer[2],Src_Pointer[5],Src_Pointer[4]);
              if (!isdigitc(time[0]) || !isdigitc(time[1]) || !isdigitc(time[3]) || !isdigitc(time[4]) || !isdigitc(time[6]) || !isdigitc(time[7]))
              {
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid character(s) in time of Service Centre Time Stamp: \"%s\"", time);
                *time = 0;
              }
              else if (atoi(time) > 23 || atoi(time +3) > 59 || atoi(time +6) > 59)
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid value(s) in time of Service Centre Time Stamp: \"%s\"", time);
                // *time = 0;
                add_warning(warning_headers, "Invalid values(s) in time of Service Centre Time Stamp.");
              }

              if (!(*err_str))
              {
                Src_Pointer += 6;
                // Time zone is not used but bytes are checked:
                if (octet2bin_check(Src_Pointer) < 0)
                  pdu_error(err_str, 0, Src_Pointer -full_pdu, 2,
                            "Invalid character(s) in Time Zone of Service Centre Time Stamp: \"%.2s\"", Src_Pointer);
                else
                  Src_Pointer += 2;
              }
            }
          }
        }
      }
    }

    if (!(*err_str))
    {
      // Src_Pointer now points to the User data length, which octet exists.
      // TODO: Can udh-len be zero?

      if (*alphabet <= 0)
      {
        if ((result = pdu2text(Src_Pointer, message, message_length, expected_length, with_udh, udh_data, udh_type, &errorpos)) < 0)
          pdu_error(err_str, 0, Src_Pointer -full_pdu +errorpos, 0, "While reading TP-UD (GSM text): %s",
                    (result == -1)? err_pdu_content : err_too_short);
      }
      else
      {
        // With binary messages udh is NOT taken from the PDU.
        i = with_udh;
        // 3.1.5: it should work now:
        if (bin_udh == 0)
          if (*alphabet == 1)
            i = 0;
        if ((result = pdu2binary(Src_Pointer, message, message_length, expected_length, i, udh_data, udh_type, &errorpos)) < 0)
          pdu_error(err_str, 0, Src_Pointer -full_pdu +errorpos, 0, "While reading TP-UD (%s): %s",
                    (*alphabet == 1)? "binary" : "UCS2 text",
                    (result == -1)? err_pdu_content : err_too_short);
      }
    }
  }

  return result;
}

// Subroutine for messages type 2 (Status Report)
// Input: 
// Src_Pointer points to the PDU string
// Output:
// sendr Sender
// date and time Date/Time-stamp
// result is the status value and text translation
int split_type_2(char *full_pdu, char* Src_Pointer,char* sendr, char* date,char* time,char* result,
                 char *from_toa, char **err_str, char *warning_headers)
{
  int Length;
  int padding;
  int status;
  char temp[32];
  char tmpsender[100];
  int messageid;
  int i;
  const char SR_MessageId[] = "Message_id:"; // Fixed title inside the status report body.
  const char SR_Status[] = "Status:"; // Fixed title inside the status report body.

  strcat(result,"SMS STATUS REPORT\n");

  // There should be at least message-id, address-length and address-type:
  if (strlen(Src_Pointer) < 6)
    pdu_error(err_str, 0, Src_Pointer -full_pdu, 6,
              "While trying to read message id, address length and address type: %s", err_too_short);
  else
  {
    // get message id
    if ((messageid = octet2bin_check(Src_Pointer)) < 0)
      pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid message id: \"%.2s\"", Src_Pointer);
    else
    {
      sprintf(strchr(result, 0), "%s %i\n", SR_MessageId, messageid);
      // get recipient address
      Src_Pointer+=2;
      Length = octet2bin_check(Src_Pointer);
      if (Length < 1 || Length > MAX_ADDRESS_LENGTH)
        pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid recipient address length: \"%.2s\"", Src_Pointer);
      else
      {
        padding=Length%2;
        Src_Pointer+=2;
        i = explain_toa(from_toa, Src_Pointer, 0);
        if (i < 0)
          pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid recipient address type: \"%.2s\"", Src_Pointer);
        else if (i < 0x80)
          pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Missing bit 7 in recipient address type: \"%.2s\"", Src_Pointer);
        else
        {
          Src_Pointer += 2;
          if ((i & 112) == 80)
          {  // Sender is alphanumeric
            if (strlen(Src_Pointer) < (size_t)(Length +padding))
              pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                        "While trying to read recipient address (alphanumeric, length %i): %s",
                        Length +padding, err_too_short);
            else
            {
              snprintf(tmpsender,Length+3+padding,"%02X%s",Length*4/7,Src_Pointer);
              if (pdu2text0(tmpsender, sendr) < 0)
                pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                          "While reading alphanumeric recipient address: %s", err_pdu_content);
            }
          }
          else
          {  // sender is numeric
            if (strlen(Src_Pointer) < (size_t)(Length +padding))
              pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding,
                        "While trying to read recipient address (numeric, length %i): %s",
                        Length +padding, err_too_short);
            else
            {
              strncpy(sendr,Src_Pointer,Length+padding);
              sendr[Length +padding] = 0;
              swapchars(sendr);
              i = Length +padding -1;
              if (padding)
              {
                if (sendr[i] != 'F')
                  add_warning(warning_headers, "Length of numeric recipient address is odd, but not terminated with 'F'.");
                else
                  sendr[i] = 0;
              }
              else
              {
                if (sendr[i] == 'F')
                {
                  add_warning(warning_headers, "Length of numeric recipient address is even, but still was terminated with 'F'.");
                  sendr[i] = 0;
                }
              }

              for (i = 0; sendr[i]; i++)
                if (!isdigitc(sendr[i]))
                {
                  // Not a fatal error (?)
                  //pdu_error(err_str, 0, Src_Pointer -full_pdu, Length +padding, "Invalid character(s) in recipient address: \"%s\"", sendr);
                  // *sendr = 0;
                  add_warning(warning_headers, "Invalid character(s) in recipient address.");
                  break;
                }
            }
          }

          if (!(*err_str))
          {
            Src_Pointer+=Length+padding;
            if (strlen(Src_Pointer) < 14)
              pdu_error(err_str, 0, Src_Pointer -full_pdu, 14, "While trying to read SMSC Timestamp: %s", err_too_short);
            else
            {
              // get SMSC timestamp
              sprintf(date,"%c%c-%c%c-%c%c",Src_Pointer[1],Src_Pointer[0],Src_Pointer[3],Src_Pointer[2],Src_Pointer[5],Src_Pointer[4]);
              if (!isdigitc(date[0]) || !isdigitc(date[1]) || !isdigitc(date[3]) || !isdigitc(date[4]) || !isdigitc(date[6]) || !isdigitc(date[7]))
              {
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid character(s) in date of SMSC Timestamp: \"%s\"", date);
                *date = 0;
              }
              else if (atoi(date +3) > 12 || atoi(date +6) > 31)
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid value(s) in date of SMSC Timestamp: \"%s\"", date);
                // *date = 0;
                add_warning(warning_headers, "Invalid value(s) in date of SMSC Timestamp.");
              }

              Src_Pointer += 6;
              sprintf(time,"%c%c:%c%c:%c%c",Src_Pointer[1],Src_Pointer[0],Src_Pointer[3],Src_Pointer[2],Src_Pointer[5],Src_Pointer[4]);
              if (!isdigitc(time[0]) || !isdigitc(time[1]) || !isdigitc(time[3]) || !isdigitc(time[4]) || !isdigitc(time[6]) || !isdigitc(time[7]))
              {
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid character(s) in time of SMSC Timestamp: \"%s\"", time);
                *time = 0;
              }
              else if (atoi(time) > 23 || atoi(time +3) > 59 || atoi(time +6) > 59)
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, 6, "Invalid value(s) in time of SMSC Timestamp: \"%s\"", time);
                // *time = 0;
                add_warning(warning_headers, "Invalid value(s) in time of SMSC Timestamp.");
              }

              if (!(*err_str))
              {
                Src_Pointer += 6;
                // Time zone is not used but bytes are checked:
                if (octet2bin_check(Src_Pointer) < 0)
                  pdu_error(err_str, 0, Src_Pointer -full_pdu, 2,
                            "Invalid character(s) in Time Zone of SMSC Time Stamp: \"%.2s\"", Src_Pointer);
                else
                  Src_Pointer += 2;
              }
            }
          }

          if (!(*err_str))
          {
            if (strlen(Src_Pointer) < 14)
              pdu_error(err_str, 0, Src_Pointer -full_pdu, 14, "While trying to read Discharge Timestamp: %s", err_too_short);
            else
            {
              // get Discharge timestamp
              sprintf(temp,"%c%c-%c%c-%c%c %c%c:%c%c:%c%c",Src_Pointer[1],Src_Pointer[0],Src_Pointer[3],Src_Pointer[2],Src_Pointer[5],Src_Pointer[4],Src_Pointer[7],Src_Pointer[6],Src_Pointer[9],Src_Pointer[8],Src_Pointer[11],Src_Pointer[10]);
              if (!isdigitc(temp[0]) || !isdigitc(temp[1]) || !isdigitc(temp[3]) || !isdigitc(temp[4]) || !isdigitc(temp[6]) || !isdigitc(temp[7]) || 
                  !isdigitc(temp[9]) || !isdigitc(temp[10]) || !isdigitc(temp[12]) || !isdigitc(temp[13]) || !isdigitc(temp[15]) || !isdigitc(temp[16]))
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 12, "Invalid character(s) in Discharge Timestamp: \"%s\"", temp);
              else if (atoi(temp +3) > 12 || atoi(temp +6) > 31 || atoi(temp +9) > 24 || atoi(temp +12) > 59 || atoi(temp +16) > 59)
              {
                // Not a fatal error (?)
                //pdu_error(err_str, 0, Src_Pointer -full_pdu, 12, "Invalid value(s) in Discharge Timestamp: \"%s\"", temp);
                add_warning(warning_headers, "Invalid values(s) in Discharge Timestamp.");
              }

              if (!(*err_str))
              {
                Src_Pointer += 12;
                // Time zone is not used but bytes are checked:
                if (octet2bin_check(Src_Pointer) < 0)
                  pdu_error(err_str, 0, Src_Pointer -full_pdu, 2,
                            "Invalid character(s) in Time Zone of Discharge Time Stamp: \"%.2s\"", Src_Pointer);
                else
                  Src_Pointer += 2;
              }
            }

            if (!(*err_str))
            {
              sprintf(strchr(result, 0), "Discharge_timestamp: %s", temp);
              if (strlen(Src_Pointer) < 2)
                pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "While trying to read Status octet: %s", err_too_short);
              else
              {
                // get Status
                if ((status = octet2bin_check(Src_Pointer)) < 0)
                  pdu_error(err_str, 0, Src_Pointer -full_pdu, 2, "Invalid Status octet: \"%.2s\"", Src_Pointer);
                else
                {
                  char buffer[128];

                  explain_status(buffer, sizeof(buffer), status);
                  sprintf(strchr(result, 0), "\n%s %i,%s", SR_Status, status, buffer);
                }
              }
            }
          }
        }
      }
    }
  }

  return strlen(result);
}

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
// with_udh return the udh flag of the message
// is_statusreport is 1 if this was a status report
// is_unsupported_pdu is 1 if this pdu was not supported
// Returns the length of the message 
int splitpdu(char *pdu, char *mode, int *alphabet, char *sendr, char *date, char *time, char *message,
             char *smsc, int *with_udh, char *a_udh_data, char *a_udh_type, int *is_statusreport,
             int *is_unsupported_pdu, char *from_toa, int *report, int *replace, char *warning_headers,
             int *flash, int bin_udh)
{
  int Length;
  int Type;
  char* Pointer;
  int result = 0;
  char *err_str = NULL;
  char *save_err_str = NULL;
  char *try_mode = mode;
  int try_count;
  int i;
  int message_length;
  int expected_length;
  char tmp_udh_data[SIZE_UDH_DATA] = {};
  char tmp_udh_type[SIZE_UDH_TYPE] = {};
  char *udh_data;
  char *udh_type;

  udh_data = (a_udh_data)? a_udh_data : tmp_udh_data;
  udh_type = (a_udh_type)? a_udh_type : tmp_udh_type;

  // Patch for Wavecom SR memory reading:
  if (strncmp(pdu, "000000FF00", 10) == 0)
  {
    strcpyo(pdu, pdu +8);
    while (strlen(pdu) < 52)
      strcat(pdu, "00");
  }
  // ------------------------------------

  for (try_count = 0; try_count < 2; try_count++)
  {
    if (try_count)
    {
      if (strcmp(mode, "new") == 0)
        try_mode = "old";
      else
        try_mode = "new";
    }

    message_length = 0;
    expected_length = 0;

    sendr[0]=0;
    date[0]=0;
    time[0]=0;
    message[0]=0;
    smsc[0]=0; 
    *alphabet=0;
    *with_udh=0;
    *udh_data = 0;
    *udh_type = 0;
    *is_statusreport = 0;
    *is_unsupported_pdu = 0;
    from_toa[0] = 0;
    *report = 0;
    *replace = -1;
    *flash = 0;

    if (warning_headers)
      *warning_headers = 0;
#ifdef DEBUGMSG
  printf("!! splitpdu(pdu=%s, mode=%s, ...)\n",pdu,mode);
#endif

    Pointer=pdu;

    if (strlen(Pointer) < 2)
      pdu_error(&err_str, 0, Pointer -pdu, 2, "While trying to read first octet: %s", err_too_short);
    else
    {
      if (strcmp(try_mode, "new") == 0)
      {
        if ((Length = octet2bin_check(Pointer)) < 0)
          pdu_error(&err_str, 0, Pointer -pdu, 2, "While reading first octet: %s", err_pdu_content);
        else
        {
          // smsc number is not mandatory
          if (Length == 0)
            Pointer += 2;
          else
          {
            // Address type and at least one octet is expected:
            if (Length < 2 || Length > MAX_SMSC_ADDRESS_LENGTH)
              pdu_error(&err_str, 0, Pointer -pdu, 2, "Invalid sender SMSC address length: \"%.2s\"", Pointer);
            else
            {
              Length = Length *2 -2;
              // No padding because the given value is number of octets.
              if (strlen(Pointer) < (size_t)(Length +4))
                pdu_error(&err_str, 0, Pointer -pdu, Length +4, "While trying to read sender SMSC address (length %i): %s",
                          Length, err_too_short);
              else
              {
                Pointer += 2;
                i = octet2bin_check(Pointer);
                if (i < 0)
                  pdu_error(&err_str, 0, Pointer -pdu, 2, "Invalid sender SMSC address type: \"%.2s\"", Pointer);
                else if (i < 0x80)
                  pdu_error(&err_str, 0, Pointer -pdu, 2, "Missing bit 7 in sender SMSC address type: \"%.2s\"", Pointer);
                else
                {
                  Pointer += 2;
                  strncpy(smsc, Pointer, Length);
                  smsc[Length] = 0;
                  swapchars(smsc);
                  // Does any SMSC use alphanumeric number?
                  if ((i & 112) == 80)
                  {
                    // There can be only hex digits from the original PDU.
                    // The number itself is wrong in this case.
                    for (i = 0; smsc[i]; i++)
                      if (!isXdigit(smsc[i]))
                      {
                        pdu_error(&err_str, 0, Pointer -pdu, Length, "Invalid character(s) in alphanumeric SMSC address: \"%s\"", smsc);
                        *smsc = 0;
                        break;
                      }
                  }
                  else
                  {
                    // Last character is allowed as F (and dropped) but all other non-numeric will produce an error:
                    if (smsc[Length -1] == 'F')
                      smsc[Length -1] = 0;
                    for (i = 0; smsc[i]; i++)
                      if (!isdigitc(smsc[i]))
                      {
                        // Not a fatal error (?)
                        //pdu_error(&err_str, 0, Pointer -pdu, Length, "Invalid character(s) in numeric SMSC address: \"%s\"", smsc);
                        // *smsc = 0;
                        add_warning(warning_headers, "Invalid character(s) in numeric SMSC address");
                        break;
                      }
                  }

                  if (!err_str)
                    Pointer += Length;
                }
              }
            }
          }
        }
      }

      if (!err_str)
      {
        if (strlen(Pointer) < 2)
          pdu_error(&err_str, 0, Pointer -pdu, 2, "While trying to read First octet of the SMS-DELIVER PDU: %s", err_too_short);
        else
        {
          if ((i = octet2bin_check(Pointer)) < 0)
            pdu_error(&err_str, 0, Pointer -pdu, 2, "While reading First octet of the SMS-DELIVER PDU: %s", err_pdu_content);
          else
          {
            // Unused bits 3 and 4 should be zero, failure with this produces a warning:
            if (i & 0x18)
              add_warning(warning_headers, "Unused bits 3 and 4 are used in the first octet of the SMS-DELIVER PDU.");

            if (i & 0x40) // Is UDH bit set?
              *with_udh = 1;
            if (i & 0x20) // Is status report going to be returned to the SME?
              *report = 1;

            Type = i & 3;
            if (Type == 0) // SMS Deliver
            {
              Pointer += 2;
              result = split_type_0(pdu, Pointer, alphabet, sendr, date, time, message, &message_length, &expected_length,
                                    *with_udh, udh_data, udh_type, from_toa, replace, &err_str, warning_headers, flash, bin_udh);

              if (err_str && *udh_type)
                pdu_error(&err_str, "", -1, 0, "Message has Udh_type: %s", udh_type);

              // If a decoding fails, the reason is invalid or missing characters 
              // in the PDU. Can also be too high TP-UDL value. 
              // Decoders are modified to return partially decoded strings with an
              // additional length variables. Binary messages are not shown in the report.
              if (*alphabet != 1 && err_str && message_length > 0)
              {
                char ascii[MAXTEXT];
                char title[101];

                if (*alphabet <= 0)
                  i = gsm2iso(message, message_length, ascii, sizeof(ascii));
                else
                {
                  memcpy(ascii, message, message_length);
#ifndef USE_ICONV
                  ascii[message_length] = 0;
                  i = decode_ucs2(ascii, message_length);
#else
                  i = (int)iconv_ucs2utf(ascii, message_length, sizeof(ascii));
#endif
                  expected_length /= 2;
                }

                if (i > 0)
                {
                  sprintf(title, "Partial content of text (%i characters, expected %i):\n", i, expected_length);
                  pdu_error(&err_str, title, -1, 0, "%s", ascii);
                }
              }
            }
            else if (Type == 2) // Status Report
            {
              Pointer += 2;
              result = split_type_2(pdu, Pointer, sendr, date, time, message, from_toa, &err_str, warning_headers);
              *is_statusreport=1;
            }
            else if (Type == 1) // Sent message
            {
              pdu_error(&err_str, "", -1, 0, "%s%.2s%s", "The PDU data (", Pointer,
                        ") says that this is a sent message. Can only decode received messages.");
              *is_unsupported_pdu = 1;
            }
            else
            {
              pdu_error(&err_str, "", -1, 0, "%s%.2s%s%i%s", "The PDU data (", Pointer, ") says that the message format is ",
                        Type, " which is not supported. Cannot decode.");
              *is_unsupported_pdu = 1;
            }
          }
        }
      }
    }

    if (!err_str)
      try_count++; // All ok, no more tries required.
    else
    {
      *alphabet = 0;
      *with_udh = 0;
      // Possible udh_data is now incorrect:
      *udh_data = 0;
      *udh_type = 0;
      *is_statusreport = 0;

      if (try_count == 0)
      {
        // First try. Save the result and try again with another PDU mode.
        if ((save_err_str = (char *)malloc(strlen(err_str) +1)))
          strcpy(save_err_str, err_str);
      }
      else
      {
        // Second try. Nothing more to do. Return report and some information.
        char *n_mode = "new (with CSA)";
        char *o_mode = "old (without CSA)";

        *message = 0;
        if (save_err_str)
          sprintf(message, "First tried with PDU mode %s:\n%s\nNext ", (*mode == 'n')? n_mode : o_mode, save_err_str);

        sprintf(strchr(message, 0), "tried with PDU mode %s:\n%s\n", (*mode == 'n')? o_mode : n_mode, err_str);
        sprintf(strchr(message, 0), "No success. This PDU cannot be decoded. There is something wrong.\n");

        if (!(*is_unsupported_pdu))
          strcat(message, "\nIf you are unsure, confused or angry, please view the GSM 03.40\n"
                          "(ETSI TS 100 901) and related documents for details of correct\n"
                          "PDU format. You can also get some help via the Support Website.\n");
        *is_unsupported_pdu = 1;
      }

      result = strlen(message);
      free(err_str);
      err_str = NULL;
    }
  }

  if (save_err_str)
    free(save_err_str);

  return result;
}

int get_pdu_details(char *dest, size_t size_dest, char *pdu, int mnumber)
{
  int result = 0;
  int udlen;
  int alphabet;
  char sender[100];
  char date[9];
  char time[9];
  char ascii[MAXTEXT];
  char smsc[31];
  int with_udh;
  char udh_data[SIZE_UDH_DATA];
  char udh_type[SIZE_UDH_TYPE];
  int is_statusreport;
  int is_unsupported_pdu;
  char from_toa[51];
  int report;
  int replace;
  char warning_headers[SIZE_WARNING_HEADERS];
  int flash;
  int bin_udh = 1;
  int m_id = 999; // real id for concatenated messages only, others use this.
  size_t length_sender = 32;
  int p_count = 1;
  int p_number = 1;
  char buffer[100]; // 3.1.6: increased size, was too small (51).
  int i;
  char sort_ch;
  char *p;

  udlen = splitpdu(pdu, DEVICE.mode, &alphabet, sender, date, time, ascii, smsc, &with_udh, udh_data, udh_type,
                   &is_statusreport, &is_unsupported_pdu, from_toa, &report, &replace, warning_headers, &flash, bin_udh);

  if (is_unsupported_pdu)
  {
    writelogfile(LOG_ERR, 1, "Message %i, unsupported PDU.", mnumber);
    result = 1;
  }
  else
  {
    if (with_udh)
    {
      if (get_remove_concatenation(udh_data, &m_id, &p_count, &p_number) < 0)
      {
        writelogfile(LOG_ERR, 1, "Message %i, error while checking UDH_DATA.", mnumber);
        result = 2;
      }
    }

    if (!result)
    {
      if (strlen(sender) > length_sender)
      {
        writelogfile(LOG_ERR, 1, "Message %i, too long sender field.", mnumber);
        result = 3;
      }    

      while (strlen(sender) < length_sender)
        strcat(sender, " ");

      if (DEVICE.priviledged_numbers[0])
        p = DEVICE.priviledged_numbers;
      else
      if (priviledged_numbers[0])
        p = priviledged_numbers;
      else
        p = 0;

      sort_ch = 'Z';
      if (p)
      {
        i = 0;
        while (*p)
        {
          if (strncmp(sender, p, strlen(p)) == 0)
          {
            sort_ch = 'A' +i;
            break;
          }
          i++;
          p = strchr(p, 0) +1;
        }
      }
/*
001 A 358401234567____________________111 001/001 r 00-00-00 00-00-00n
mnumber
    sort_ch
      sender
                                      m_id
                                          p_number
                                              p_count
                                                  incoming / report
                                                    date
                                                             time

      snprintf(buffer, sizeof(buffer), "%.03i %c %s%.03i %.03i/%.03i %c %-8.8s %-8.8s\n",
               mnumber, sort_ch, sender, m_id, p_number, p_count, (is_statusreport) ? 'r' : 'i', date, time);

3.1.7:
001 A 00-00-00 00-00-00 358401234567____________________111 001/001 rn
mnumber
    sort_ch
      date
               time
                        sender
                                                        m_id
                                                            p_number
                                                                p_count
                                                                    incoming / report
*/
      snprintf(buffer, sizeof(buffer), "%.03i %c %-8.8s %-8.8s %s%.03i %.03i/%.03i %c\n", mnumber, sort_ch, date, time, sender, m_id, p_number, p_count, (is_statusreport) ? 'r' : 'i');

      if (strlen(dest) +strlen(buffer) < size_dest)
        strcat(dest, buffer);
      else
      {
        writelogfile(LOG_ERR, 1, "Message %i, not enough storage space.", mnumber);
        result = 4;
      }
    }
  }

  return result;
}

int sort_pdu_helper(const void *a, const void *b)
{
  //return(strncmp((char *)a +4, (char *)b +4, 45));
  return (strncmp((char *) a + 4, (char *) b + 4, 63));
}

void sort_pdu_details(char *dest)
{
  int count;

  count = strlen(dest) / LENGTH_PDU_DETAIL_REC;
  if (count > 1)
    qsort((void *)dest, count, LENGTH_PDU_DETAIL_REC, sort_pdu_helper);
}
