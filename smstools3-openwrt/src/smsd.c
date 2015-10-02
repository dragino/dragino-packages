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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#ifndef NOSTATS
#include <mm.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include "extras.h"
#include "locking.h"
#include "smsd_cfg.h"
#include "stats.h"
#include "version.h"
#include "blacklist.h"
#include "whitelist.h"
#include "logging.h"
#include "alarm.h"
#include "charset.h"
#include "cfgfile.h"
#include "pdu.h"
#include "modeminit.h"

int logfilehandle;  // handle of log file.
int concatenated_id=0; // id number for concatenated messages.
// This indicates that the PDU was read from file, not from SIM.
#define PDUFROMFILE 22222
int break_workless_delay; // To break the delay when SIGCONT is received.
int workless_delay;

int break_suspend; // To break suspend when SIGUSR2 is received.

const char *HDR_To =			"To:";		// Msg file input.
char HDR_To2[SIZE_HEADER] = {};
const char *HDR_ToTOA =                 "To_TOA:";      // Msg file input. Type Of Address (numbering plan): unknown (0), international (1), national (2).
char HDR_ToTOA2[SIZE_HEADER] = {};
const char *HDR_From =			"From:";	// Msg file input: informative, incoming message: senders address.
char HDR_From2[SIZE_HEADER] = {};
const char *HDR_Flash =			"Flash:";	// Msg file input.
char HDR_Flash2[SIZE_HEADER] = {};
const char *HDR_Provider =		"Provider:";	// Msg file input.
char HDR_Provider2[SIZE_HEADER] = {};
const char *HDR_Queue =			"Queue:";	// Msg file input.
char HDR_Queue2[SIZE_HEADER] = {};
const char *HDR_Binary =		"Binary:";	// Msg file input (sets alphabet to 1 or 0).
char HDR_Binary2[SIZE_HEADER] = {};
const char *HDR_Report =		"Report:";	// Msg file input. Incoming message: report was asked yes/no.
char HDR_Report2[SIZE_HEADER] = {};
const char *HDR_Autosplit =		"Autosplit:";	// Msg file input.
char HDR_Autosplit2[SIZE_HEADER] = {};
const char *HDR_Validity =		"Validity:";	// Msg file input.
char HDR_Validity2[SIZE_HEADER] = {};
const char *HDR_Voicecall =		"Voicecall:";	// Msg file input.
char HDR_Voicecall2[SIZE_HEADER] = {};
const char *HDR_Replace =		"Replace:";	// Msg file input. Incoming message: exists with code if replace
char HDR_Replace2[SIZE_HEADER] = {};			// code was defined.
const char *HDR_Alphabet =		"Alphabet:";	// Msg file input. Incoming message.
char HDR_Alphabet2[SIZE_HEADER] = {};
const char *HDR_Include =		"Include:";	// Msg file input.
char HDR_Include2[SIZE_HEADER] = {};
const char *HDR_Macro =			"Macro:";	// Msg file input.
char HDR_Macro2[SIZE_HEADER] = {};
const char *HDR_Hex =			"Hex:";		// Msg file input.
char HDR_Hex2[SIZE_HEADER] = {};
const char *HDR_SMSC =			"SMSC:";	// Msg file input: smsc number.
char HDR_SMSC2[SIZE_HEADER] = {};
const char *HDR_Priority =		"Priority:";	// Msg file input.
char HDR_Priority2[SIZE_HEADER] = {};
const char *HDR_System_message =	"System_message:"; // Msg file input.
char HDR_System_message2[SIZE_HEADER] = {};
const char *HDR_Smsd_debug =		"Smsd_debug:";	// For debugging purposes
char HDR_Smsd_debug2[SIZE_HEADER] = {};

const char *HDR_UDHDATA =		"UDH-DATA:";	// Msg file input. Incoming message.
const char *HDR_UDHDUMP =		"UDH-DUMP:";	// Msg file input (for backward compatibility).
const char *HDR_UDH =			"UDH:";		// Msg file input. Incoming binary message: "yes" / "no".

const char *HDR_Sent =			"Sent:";	// Outgoing timestamp, incoming: senders date & time (from PDU).
char HDR_Sent2[SIZE_HEADER] = {};
const char *HDR_Modem =			"Modem:";	// Sent message, device name (=modemname). After >= 3.1.4 also incoming message.
char HDR_Modem2[SIZE_HEADER] = {};
const char *HDR_Number =		"Number:";	// 3.1.4: Sent message, incoming message, SIM card's telephone number.
char HDR_Number2[SIZE_HEADER] = {};
const char *HDR_FromTOA =		"From_TOA:";	// Incoming message: senders Type Of Address.
char HDR_FromTOA2[SIZE_HEADER] = {};
const char *HDR_FromSMSC =		"From_SMSC:";	// Incoming message: senders SMSC
char HDR_FromSMSC2[SIZE_HEADER] = {};
const char *HDR_Name =			"Name:";	// Incoming message: name from the modem response (???).
char HDR_Name2[SIZE_HEADER] = {};
const char *HDR_Received =		"Received:";	// Incoming message timestamp.
char HDR_Received2[SIZE_HEADER] = {};
const char *HDR_Subject =		"Subject:";	// Incoming message, modemname.
char HDR_Subject2[SIZE_HEADER] = {};
const char *HDR_UDHType =		"UDH-Type:";	// Incoming message, type(s) of content of UDH if present.
char HDR_UDHType2[SIZE_HEADER] = {};
const char *HDR_Length =		"Length:";	// Incoming message, text/data length. With Unicode: number of Unicode
char HDR_Length2[SIZE_HEADER] = {};			// characters. With GSM/ISO: nr of chars, may differ if stored as UTF-8.
const char *HDR_FailReason =		"Fail_reason:";	// Failed outgoing message, error text.
char HDR_FailReason2[SIZE_HEADER] = {};
const char *HDR_Failed =		"Failed:";	// Failed outgoing message, timestamp.
char HDR_Failed2[SIZE_HEADER] = {};
const char *HDR_Identity =		"IMSI:";	// Incoming / Sent(or failed), exists with code if IMSI request
char HDR_Identity2[SIZE_HEADER] = {};			// supported.
const char *HDR_MessageId =		"Message_id:";	// Sent (successfully) message. There is fixed "message id" and
char HDR_MessageId2[SIZE_HEADER] = {};			// "status" titled inside the body of status report.
const char *HDR_OriginalFilename =	"Original_filename:";	// Stored when moving file from outgoing directory and
char HDR_OriginalFilename2[SIZE_HEADER] = {};			// unique filenames are used in the spooler.
const char *HDR_CallType =		"Call_type:";	// Incoming message from phonebook.
char HDR_CallType2[SIZE_HEADER] = {};
const char *HDR_missed =		"missed";	// For incoming call type.
char HDR_missed2[SIZE_HEADER] = {};
const char *HDR_missed_text =		"CALL MISSED";	// For incoming call, message body.
char HDR_missed_text2[SIZE_HEADER] = {};
const char *HDR_Result =		"Result:";	// For voice call, result string from a modem
char HDR_Result2[SIZE_HEADER] = {};
const char *HDR_Incomplete =		"Incomplete:";	// For purged message files.
char HDR_Incomplete2[SIZE_HEADER] = {};

char *EXEC_EVENTHANDLER =	"eventhandler";
char *EXEC_RR_MODEM =		"regular_run (modem)";
char *EXEC_RR_POST_MODEM =	"regular_run_post_run (modem)";
char *EXEC_RR_MAINPROCESS =	"regular_run (mainprocess)";
char *EXEC_CHECKHANDLER =	"checkhandler";

// Prototype needed:
void send_admin_message(int *quick, int *errorcounter, char *text);

void sendsignal2devices(int signum)
{
  int i;

  for (i = 0; i < NUMBER_OF_MODEMS; i++)
    if (device_pids[i] > 0)
      kill(device_pids[i], signum);
}

int exec_system(char *command, char *info)
{
  int result;
  int *i = 0;
  char *to = NULL;
  //static int last_status_eventhandler = -1;
  //static int last_status_rr_modem = -1;
  //static int last_status_rr_post_modem = -1;
  //static int last_status_rr_mainprocess = -1;
  static int last_status = -1; // One status for each process

  result = my_system(command, info);

  if (!strcmp(info, EXEC_EVENTHANDLER))
    i = &last_status; //_eventhandler;
  else
  if (!strcmp(info, EXEC_RR_MODEM))
    i = &last_status; //_rr_modem;
  else
  if (!strcmp(info, EXEC_RR_POST_MODEM))
    i = &last_status; //_rr_post_modem;
  else
  if (!strcmp(info, EXEC_RR_MAINPROCESS))
    i = &last_status; //_rr_mainprocess;

  if (i)
  {
    if (!result)
      *i = 0; // running was ok
    else
    {
      char alert[256];

      snprintf(alert, sizeof(alert), "problem with %s, result %i", info, result);

      if (process_id >= 0)
      {
        // Modems only.
        if (DEVICE.admin_to[0])
          to = DEVICE.admin_to;
        else if (admin_to[0])
          to = admin_to;
      }

      // If last result was ok or unknown, alert is sent.
      if (*i <= 0)
      {
        int timeout = 0;

        writelogfile(LOG_ERR, 1, "ALERT: %s", alert);

        if (to)
        {
          char msg[256];
          int quick = 0;
          int errorcounter = 0;

          snprintf(msg, sizeof(msg), "Smsd3: %s, %s", DEVICE.name, alert);
          send_admin_message(&quick, &errorcounter, msg);
        }
        else
        {
          if (*adminmessage_device && process_id == -1 && shared_buffer)
          {
            time_t start_time;

            start_time = time(0);

            // Buffer should be free:
            while (*shared_buffer)
            {
              if (time(0) -start_time > 60)
                break;
              sendsignal2devices(SIGCONT);
              t_sleep(5);
            }

            if (*shared_buffer)
            {
              timeout = 1;
              writelogfile(LOG_INFO, 1, "Timeout while trying to deliver alert to %s", adminmessage_device);
            }
            else
            {
              snprintf(shared_buffer, SIZE_SHARED_BUFFER, "%s Sms3: mainprocess, %s", adminmessage_device, alert);
              sendsignal2devices(SIGCONT);
            }
          }
        }

        if (timeout)
          *i = -1; // retry next time if error remains
        else
          *i = 1; // running failed
      }
      else
      {
        (*i)++;
        writelogfile(LOG_INFO, 1, "ALERT (continues, %i): %s", *i, alert);
      }
    }
  }

  return result;
}

int read_translation(void)
{
  int result = 0; // Number of problems
  FILE *fp;
  char name[32];
  char value[PATH_MAX];
  int getline_result;
  char *p;
  int i;

  if (*language_file)
  {
    if (!(fp = fopen(language_file, "r")))
    {
      fprintf(stderr, "%s\n", tb_sprintf("Cannot read language file %s: %s", language_file, strerror(errno)));
      writelogfile(LOG_CRIT, 1, "%s", tb);
      result++;
    }
    else
    {
      while ((getline_result = my_getline(fp, name, sizeof(name), value, sizeof(value))) != 0)
      {
        if (getline_result == -1)
        {
          fprintf(stderr, "%s\n", tb_sprintf("Syntax Error in language file: %s", value));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (line_is_blank(value))
        {
          fprintf(stderr, "%s\n", tb_sprintf("%s has no value in language file.", name));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (strlen(value) >= SIZE_HEADER)
        {
          fprintf(stderr, "%s\n", tb_sprintf("Too long value for %s in language file: %s", name, value));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (*value == '-')
        {
          while (value[1] && strchr(" \t", value[1]))
            strcpyo(value +1, value +2);

          if (!strcasecmp(name, HDR_From) ||
              !strcasecmp(name, HDR_Received) ||
              !strcasecmp(name, HDR_missed) ||
              !strcasecmp(name, HDR_missed_text))
          {
            fprintf(stderr, "%s\n", tb_sprintf("In language file, translation for %s cannot start with '-' character", name));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }
        }

        if (!strcasecmp(name, "incoming"))
          translate_incoming = yesno(value);
        else if (!strcasecmp(name, "datetime"))
        {
          // Not much can be checked, only if it's completelly wrong...
          char timestamp[81];
          time_t now;

          time(&now);
          if (!strchr(value, '%') ||
              strftime(timestamp, sizeof(timestamp), value, localtime(&now)) == 0 ||
              !strcmp(timestamp, value))
          {
            fprintf(stderr, "%s\n", tb_sprintf("In language file, format for datetime is completelly wrong: \"%s\"", value));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }
          else
            strcpy(datetime_format, value);
        }
        else if (!strcasecmp(name, "yes_word"))
          strcpy(yes_word, value);
        else if (!strcasecmp(name, "no_word"))
          strcpy(no_word, value);
        else if (!strcasecmp(name, "yes_chars") || !strcasecmp(name, "no_chars"))
        {
          // Every possible character (combination) is given between apostrophes.
          // This is because one UTF-8 character can be represented using more than on byte.
          // There can be more than one definition delimited with a comma. Uppercase and
          // lowercase is handled by the definition because of UTF-8 and because of
          // non-US-ASCII character sets.
          // Example (very easy one): 'K','k'

          // First remove commas, spaces and double apostrophes:
          while ((p = strchr(value, ',')))
            strcpyo(p, p +1);
          while ((p = strchr(value, ' ')))
            strcpyo(p, p +1);
          while ((p = strstr(value, "''")))
            strcpyo(p, p +1);

          // First apostrophe is not needed:
          if (value[0] == '\'')
            strcpyo(value, value +1);
          // Ensure that last apostrophe is there:
          if ((i = strlen(value)) > 0)
          {
            if (value[i -1] != '\'')
              if (i < SIZE_HEADER -1)
                strcat(value, "'");
          }
          else
          {
            fprintf(stderr, "%s\n", tb_sprintf("%s has an incomplete value in language file", name));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }

          // Example (UTF-8): (byte1)(byte2)(byte3)'(byte1)(byte2)'
          // yesno() is now able to check what was meant with an answer.

          if (!strcasecmp(name, "yes_chars"))
            strcpy(yes_chars, value);
          else
            strcpy(no_chars, value);
        }
        else if (!strcasecmp(name, HDR_To))
          strcpy(HDR_To2, value);
        else if (!strcasecmp(name, HDR_ToTOA))
          strcpy(HDR_ToTOA2, value);
        else if (!strcasecmp(name, HDR_From))
          strcpy(HDR_From2, value);
        else if (!strcasecmp(name, HDR_Flash))
          strcpy(HDR_Flash2, value);
        else if (!strcasecmp(name, HDR_Provider))
          strcpy(HDR_Provider2, value);
        else if (!strcasecmp(name, HDR_Queue))
          strcpy(HDR_Queue2, value);
        else if (!strcasecmp(name, HDR_Binary))
          strcpy(HDR_Binary2, value);
        else if (!strcasecmp(name, HDR_Report))
          strcpy(HDR_Report2, value);
        else if (!strcasecmp(name, HDR_Autosplit))
          strcpy(HDR_Autosplit2, value);
        else if (!strcasecmp(name, HDR_Validity))
          strcpy(HDR_Validity2, value);
        else if (!strcasecmp(name, HDR_Voicecall))
          strcpy(HDR_Voicecall2, value);
        else if (!strcasecmp(name, HDR_Replace))
          strcpy(HDR_Replace2, value);
        else if (!strcasecmp(name, HDR_Alphabet))
          strcpy(HDR_Alphabet2, value);
        else if (!strcasecmp(name, HDR_Include))
          strcpy(HDR_Include2, value);
        else if (!strcasecmp(name, HDR_Macro))
          strcpy(HDR_Macro2, value);
        else if (!strcasecmp(name, HDR_Hex))
          strcpy(HDR_Hex2, value);
        else if (!strcasecmp(name, HDR_SMSC))
          strcpy(HDR_SMSC2, value);
        else if (!strcasecmp(name, HDR_Priority))
          strcpy(HDR_Priority2, value);
        else if (!strcasecmp(name, HDR_System_message))
          strcpy(HDR_System_message2, value);
        else if (!strcasecmp(name, HDR_Sent))
          strcpy(HDR_Sent2, value);
        else if (!strcasecmp(name, HDR_Modem))
          strcpy(HDR_Modem2, value);
        else if (!strcasecmp(name, HDR_Number))
          strcpy(HDR_Number2, value);
        else if (!strcasecmp(name, HDR_FromTOA))
          strcpy(HDR_FromTOA2, value);
        else if (!strcasecmp(name, HDR_FromSMSC))
          strcpy(HDR_FromSMSC2, value);
        else if (!strcasecmp(name, HDR_Name))
          strcpy(HDR_Name2, value);
        else if (!strcasecmp(name, HDR_Received))
          strcpy(HDR_Received2, value);
        else if (!strcasecmp(name, HDR_Subject))
          strcpy(HDR_Subject2, value);
        else if (!strcasecmp(name, HDR_UDHType))
          strcpy(HDR_UDHType2, value);
        else if (!strcasecmp(name, HDR_Length))
          strcpy(HDR_Length2, value);
        else if (!strcasecmp(name, HDR_FailReason))
          strcpy(HDR_FailReason2, value);
        else if (!strcasecmp(name, HDR_Failed))
          strcpy(HDR_Failed2, value);
        else if (!strcasecmp(name, HDR_Identity))
          strcpy(HDR_Identity2, value);
        else if (!strcasecmp(name, HDR_MessageId))
          strcpy(HDR_MessageId2, value);
        else if (!strcasecmp(name, HDR_OriginalFilename))
          strcpy(HDR_OriginalFilename2, value);
        else if (!strcasecmp(name, HDR_CallType))
          strcpy(HDR_CallType2, value);
        else if (!strcasecmp(name, HDR_missed))
          strcpy(HDR_missed2, value);
        else if (!strcasecmp(name, HDR_missed_text))
          strcpy(HDR_missed_text2, value);
        else if (!strcasecmp(name, HDR_Result))
          strcpy(HDR_Result2, value);
        else if (!strcasecmp(name, HDR_Incomplete))
          strcpy(HDR_Incomplete2, value);
        else
        {
          fprintf(stderr, "%s\n", tb_sprintf("Unknown variable in language file: %s", name));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }
      }

      fclose(fp);
    }

    if (!result)
      writelogfile(LOG_INFO, 0, "Using language file %s", language_file);
  }

  return result;
}

// Used to select an appropriate header:
char *get_header(const char *header, char *header2)
{
  if (header2 && *header2 && strcmp(header2, "-"))
  {
    if (*header2 == '-')
      return header2 +1;
    return header2;    
  }
  return (char *)header;
}

char *get_header_incoming(const char *header, char *header2)
{
  if (!translate_incoming)
    return (char *)header;
  return get_header(header, header2);
}

// Return value: 1/0
// hlen = length of a header which matched.
int test_header(int *hlen, char *line, const char *header, char *header2)
{
  // header2:
  // NULL or "" = no translation
  // "-" = no translation and header is not printed
  // "-Relatório:" = input translated, header is not printed
  // "Relatório:" = input and output translated

  if (header2 && *header2 && strcmp(header2, "-"))
  {
    if (*header2 == '-')
    {
      if (!strncmp(line, header2 +1, *hlen = strlen(header2) -1))
        return 1;
    }
    else if (!strncmp(line, header2, *hlen = strlen(header2)))
      return 1;
  }

  if (!strncmp(line, header, *hlen = strlen(header)))
    return 1;

  *hlen = 0;
  return 0;
}

int prepare_remove_headers(char *remove_headers, size_t size)
{
  char *p;

  if (snprintf(remove_headers, size, "\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\nPDU:",
               HDR_FailReason,
               HDR_Failed,
               HDR_Identity,
               HDR_Modem,
               HDR_Number,
               HDR_Sent,
               HDR_MessageId,
               HDR_Result) >= (ssize_t)size)
    return 0;

  if ((p = get_header(NULL, HDR_FailReason2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Failed2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Identity2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Modem2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Number2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Sent2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_MessageId2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Result2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  return 1;
}

/* =======================================================================
   Macros can be used with ISO/UTF-8 written messages with cs_convert=yes
   and with binary messages written with hex=yes.
   A buffer should be 0-terminated and it cannot include 0x00 characters.
   ======================================================================= */

int extract_macros(char *buffer, int buffer_size, char *macros)
{
  int result = 0;
  char *p_buffer;
  char *p_macro;
  char *p_value;
  char *p;
  int len_buffer;
  int len_macro;
  int len_value;

  if (macros && *macros)
  {
    p_macro = macros;
    while (*p_macro)
    {
      if ((p_value = strchr(p_macro, '=')))
      {
        p_value++;
        *(p_value -1) = 0; // for easier use of strstr.
        len_macro = strlen(p_macro);
        len_value = strlen(p_value);
        p_buffer = buffer;
        while ((p = strstr(p_buffer, p_macro)))
        {
          if (len_macro < len_value)
          {
            len_buffer = strlen(buffer);
            if (len_buffer -len_macro +len_value >= buffer_size)
            {
              result = 1;
              break;
            }
            memmove(p +len_value, p +len_macro, len_buffer -(p -buffer) -len_macro +1);
          }
          else if (len_macro > len_value)
            strcpyo(p +len_value, p +len_macro);

          if (len_value > 0)
          {
            strncpy(p, p_value, len_value);
            p_buffer = p +len_value;
          }
        }

        *(p_value -1) = '='; // restore delimiter.
      }
      p_macro = strchr(p_macro, 0) +1;
    }
  }

  return result;
}

/* ======================================================================= */

int apply_filename_preview(char *filename, char *arg_text, int setting_length)
{
/*
£$¥èéùìòÇØøÅå Ææ É ÄÖÑÜ§ äöñüà ¤
LSYeeuioCOoAa Aa E AONUS aonua e
*/
  char *allowed_chars = "-.";
  char replace_from[] = { 
    0xA3, '$', 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, 0xD8, 0xF8, 0xC5, 0xE5,
    0xC6, 0xE6,
    0xC9,
    0xC4, 0xD6, 0xD1, 0xDC, 0xA7,
    0xE4, 0xF6, 0xF1, 0xFC, 0xE0,
    '¤',
    0x00
  };
  char *replace_to   = "LSYeeuioCOoAaAaEAONUSaonuae";
  char old_filename[PATH_MAX];
  int i;
  char *p;
  char *text; // Fix in 3.1.7.

  if (!filename || strlen(filename) >= PATH_MAX -2)
    return 0;

  if (!arg_text || setting_length <= 0)
    return 0;

  if (!(text = strdup(arg_text)))
    return 0;

  strcpy(old_filename, filename);
  i = setting_length; //filename_preview;
  if (strlen(filename) +2 +i > PATH_MAX)
    i = PATH_MAX - strlen(filename) -2;
  if (strlen(text) > (size_t)i)
    text[i] = 0;
  for (i = 0; text[i] != 0; i++)
  {
    if (!isalnumc(text[i]) && !strchr(allowed_chars, text[i]))
    {
      if ((p = strchr(replace_from, text[i])))
        text[i] = replace_to[p -replace_from];
      else                
        text[i] = '_';
    }
  }

  strcat(filename, "-");
  strcat(filename, text);
  free(text);
  if (rename(old_filename, filename) != 0)
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot rename file %s to %s", old_filename, filename));
    alarm_handler0(LOG_ERR, tb);
    strcpy(filename, old_filename);
    return 0;
  }

  return 1;
} 

/* =======================================================================
   Runs checkhandler and returns return code:
   0 = message is accepted.
   1 = message is rejected.
   2 = message is accepted and checkhandler has moved it to correct spooler.
   ======================================================================= */
   
int run_checkhandler(char* filename)
{
  char cmdline[PATH_MAX+PATH_MAX+32];

  if (checkhandler[0])
  {
    snprintf(cmdline, sizeof(cmdline), "%s %s", checkhandler, filename);
    return my_system(cmdline, EXEC_CHECKHANDLER);
  }

  return 0;
}

/* =======================================================================
   Stops the program if the given file exists
   ======================================================================= */

/* filename1 is checked. The others arguments are used to compose an error message. */

void stop_if_file_exists(char* infotext1, char* filename1, char* infotext2, char* filename2)
{
  int datei;

  datei=open(filename1,O_RDONLY);
  if (datei>=0)
  {
    close(datei);
    writelogfile0(LOG_CRIT, 1, tb_sprintf("Fatal error: %s %s %s %s. Check file and dir permissions.",
                  infotext1, filename1, infotext2, filename2));
    alarm_handler0(LOG_CRIT, tb);
    abnormal_termination(1);
  }
}

/* =======================================================================
   Remove and add headers in a message file
   - remove_headers: "\nHeader1:\nHeader2:" (\n and : are delimiters)
   - add_buffer is an additional header data wich is added after add_headers.
     This data is not processed with format string. For example to be used
     with pdu store which can be very large and there is no reason to alloc
     memory for just passing this data.
   - add_headers: "Header1: value\nHeader2: value\n" (actual line(s) to write)
   ======================================================================= */

int change_headers(char *filename, char *remove_headers, char *add_buffer, char *add_headers, ...)
{
  int result = 0;
  char line[1024];
  char header[1024 +1];
  char *p;
  int in_headers = 1;
  char tmp_filename[PATH_MAX +7];
  FILE *fp;
  FILE *fptmp;
  size_t n;
  va_list argp;
  char new_headers[2048];

  va_start(argp, add_headers);
  vsnprintf(new_headers, sizeof(new_headers), add_headers, argp);
  va_end(argp);

  // 3.1.12: Temporary file in checked directory causes troubles with more than one modems:
  //sprintf(tmp_filename,"%s.XXXXXX", filename);
  sprintf(tmp_filename,"/tmp/smsd.XXXXXX");

  close(mkstemp(tmp_filename));
  unlink(tmp_filename);
  if (!(fptmp = fopen(tmp_filename, "w")))
  {
    writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, creating %s failed", tmp_filename));
    alarm_handler0(LOG_WARNING, tb);
    result = 1;
  }
  else
  {
    if (!(fp = fopen(filename, "r")))
    {
      fclose(fptmp);
      unlink(tmp_filename);
      writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, reading %s failed", filename));
      alarm_handler0(LOG_WARNING, tb);
      result = 2;
    }
    else
    {
      strcpy(header, "\n");
      while (in_headers && fgets(line, sizeof(line), fp))
      {
        if (remove_headers && *remove_headers)
        {
          // Possible old headers are removed:
          if ((p = strchr(line, ':')))
          {
            strncpy(header +1, line, p -line +1);
            header[p -line +2] = 0;
            if (strstr(remove_headers, header))
              continue;
          }
        }

        if (line_is_blank(line))
        {
          if (*new_headers)
            fwrite(new_headers, 1, strlen(new_headers), fptmp);
          if (add_buffer && *add_buffer)
            fwrite(add_buffer, 1, strlen(add_buffer), fptmp);
          in_headers = 0;
        }
        fwrite(line, 1, strlen(line), fptmp);
      }

      // 3.1beta7: Because of Include feature, all text can be in different file
      // and therefore a delimiter line is not in this file.
      if (in_headers)
      {
        if (*new_headers)
          fwrite(new_headers, 1, strlen(new_headers), fptmp);
        if (add_buffer && *add_buffer)
          fwrite(add_buffer, 1, strlen(add_buffer), fptmp);
      }

      while ((n = fread(line, 1, sizeof(line), fp)) > 0)
        fwrite(line, 1, n, fptmp);

      fclose(fptmp);
      fclose(fp);

      // 3.1.14: rename does not work across different mount points:
      //unlink(filename);
      //rename(tmp_filename, filename);
      if (rename(tmp_filename, filename) != 0)
      {
        if (!(fptmp = fopen(tmp_filename, "r")))
        {
            writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, reading %s failed", tmp_filename));
            alarm_handler0(LOG_WARNING, tb);
            result = 2;
        }
        else
        {
          if (!(fp = fopen(filename, "w")))
          {
            writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, creating %s failed", filename));
            alarm_handler0(LOG_WARNING, tb);
            result = 1;
          }
          else
          {
            while ((n = fread(line, 1, sizeof(line), fptmp)) > 0)
              fwrite(line, 1, n, fp);

            fclose(fp);
          }

          fclose(fptmp);
          unlink(tmp_filename);
        }
      }

    }
  }

  return result;
}

/* =======================================================================
   Read the header of an SMS file
   ======================================================================= */

void readSMSheader(char* filename, /* Filename */
                   int recursion_level,
// output variables are:
                   char* to, 		/* destination number */
	           char* from, 		/* sender name or number */
	           int*  alphabet, 	/* -1=GSM 0=ISO 1=binary 2=UCS2 3=UTF8 4=unknown */
                   int* with_udh,  	/* UDH flag */
                   char* udh_data,  	/* UDH data in hex dump format. Only used in alphabet<=0 */
	           char* queue, 	/* Name of Queue */
	           int*  flash, 	/* 1 if send as Flash SMS */
	           char* smsc, 		/* SMSC Number */
                   int*  report,  	/* 1 if request status report */
		   int*  split,  	/* 1 if request splitting */
                   int*  validity, 	/* requested validity period value */
                   int*  voicecall, 	/* 1 if request voicecall */
                   int* hex,		/* 1 if binary message is presented as hexadecimal */
                   int *replace_msg,	/* 1...7, 0=none */
                   char *macros,
                   int *system_msg,	/* 1 if sending as a system message. */
                   int *to_type)        /* -1 = default, -2 = error, >= 0 = accepted value  */
{
  FILE* File;
  char line[SIZE_UDH_DATA +256];
  char *ptr;
  int hlen;

  if (recursion_level == 0)
  {
    to[0] = 0;
    from[0] = 0;
    *alphabet = 0; 
    *with_udh = -1;
    udh_data[0] = 0;
    queue[0] = 0;
    *flash = 0;
    smsc[0] = 0;
    *report = -1; 
    *split = -1;
    *validity = -1;
    *voicecall = 0;
    *hex = 0;
    if (macros)
      *macros = 0;
    *system_msg = 0;
    *to_type = -1;

    // This is global:
    smsd_debug[0] = 0;
  }

#ifdef DEBUGMSG
  printf("!! readSMSheader(filename=%s, recursion_level:%i, ...)\n",filename, recursion_level);
#endif 
 
  if ((File = fopen(filename, "r")))
  {
    // read until end of file or until an empty line was found
    while (fgets(line,sizeof(line),File))
    {
      if (line_is_blank(line))
        break;

      if (test_header(&hlen, line, HDR_To, HDR_To2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_TO)
        {
          // correct phone number if it has wrong syntax
          if (strstr(line,"00")==line)
            strcpy(to,line+2);
          else if ((ptr=strchr(line,'+')))
            strcpy(to,ptr+1);
          else
            strcpy(to,line);

          // 3.1beta3: 
          // Allow number grouping (like "358 12 345 6789") and *# characters in any position of a phone number.
          ptr = (*to == 's')? to +1: to;
          while (*ptr)
          {
            // 3.1.1: Allow -
            if (is_blank(*ptr) || *ptr == '-')
              strcpyo(ptr, ptr +1);
            else if (!strchr("*#0123456789", *ptr))
              *ptr = 0;
            else
              ptr++;
          }
        }

        // 3.1.6: Cannot accept 's' only:
        if (!strcmp(to, "s"))
                *to = 0;
      }
      else
      if (test_header(&hlen, line, HDR_ToTOA, HDR_ToTOA2))
      {
        cutspaces(strcpyo(line, line +hlen));

        if (!strcasecmp(line, "unknown"))
          *to_type = 0;
        else if (!strcasecmp(line, "international"))
          *to_type = 1;
        else if (!strcasecmp(line, "national"))
          *to_type = 2;
        else 
          *to_type = -2;
      }
      else
      if (test_header(&hlen, line, HDR_From, HDR_From2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_FROM)
          strcpy(from, line);
      }
      else
      if (test_header(&hlen, line, HDR_SMSC, HDR_SMSC2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_SMSC)
        {
          // 3.1beta7: allow grouping:
          // 3.1.5: fixed infinite loop bug.
          ptr = line;
          while (*ptr)
          {
            if (is_blank(*ptr))
              strcpyo(ptr, ptr +1);
            else
              ptr++;
          }

          // correct phone number if it has wrong syntax
          if (strstr(line,"00")==line)
            strcpy(smsc,line+2);
          else if (strchr(line,'+')==line)
            strcpy(smsc,line+1);
          else
            strcpy(smsc,line);
        }
      }
      else
      if (test_header(&hlen, line, HDR_Flash, HDR_Flash2))
        *flash = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Provider, HDR_Provider2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_QUEUENAME)
          strcpy(queue, line);
      }
      else
      if (test_header(&hlen, line, HDR_Queue, HDR_Queue2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_QUEUENAME)
          strcpy(queue, line);
      }
      else
      if (test_header(&hlen, line, HDR_Binary, HDR_Binary2))
        *alphabet = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Report, HDR_Report2))
        *report = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Autosplit, HDR_Autosplit2))
        *split = atoi(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Validity, HDR_Validity2))
        *validity = parse_validity(cutspaces(strcpyo(line, line +hlen)), *validity);
      else
      if (test_header(&hlen, line, HDR_Voicecall, HDR_Voicecall2))
        *voicecall = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Hex, HDR_Hex2))
        *hex = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Replace, HDR_Replace2))
      {
        *replace_msg = atoi(cutspaces(strcpyo(line, line +hlen)));
        if (*replace_msg < 0 || *replace_msg > 7)
          *replace_msg = 0;
      }
      else
      if (test_header(&hlen, line, HDR_Alphabet, HDR_Alphabet2))
      {
        cutspaces(strcpyo(line, line +hlen));
        if (strcasecmp(line,"GSM")==0)
          *alphabet=-1;
        else if (strncasecmp(line,"iso",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"lat",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"ans",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"bin",3)==0)
          *alphabet=1;
        else if (strncasecmp(line,"chi",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"ucs",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"uni",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"utf",3)==0)
          *alphabet=3;
        else
          *alphabet=4;
      } 
      else
      if (strncmp(line, HDR_UDHDATA, hlen = strlen(HDR_UDHDATA)) == 0)
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_UDH_DATA)
          strcpy(udh_data, line);
      }
      else
      if (strncmp(line, HDR_UDHDUMP, hlen = strlen(HDR_UDHDUMP)) == 0) // same as UDH-DATA for backward compatibility
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_UDH_DATA)
          strcpy(udh_data, line);
      }      
      else
      if (strncmp(line, HDR_UDH, hlen = strlen(HDR_UDH)) == 0)
        *with_udh = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        cutspaces(strcpyo(line, line +hlen));
        if (recursion_level < 1)
          readSMSheader(line, recursion_level +1, to, from, alphabet, with_udh, udh_data, queue, flash,
  	              smsc, report, split, validity, voicecall, hex, replace_msg, macros, system_msg, to_type);
      }
      else
      if (test_header(&hlen, line, HDR_Macro, HDR_Macro2))
      {
        char *p, *p1, *p2;

        cut_ctrl(strcpyo(line, line +hlen));
        while (*line == ' ' || *line == '\t')
          strcpyo(line, line +1);

        if ((p1 = strchr(line, '=')) && *line != '=' && macros)
        {
          // If there is previously defined macro with the same name, it is removed first:
          p = macros;
          while (*p)
          {
            if (strncmp(p, line, 1 +(int)(p1 -line)) == 0)
            {
              p2 = strchr(p, 0) +1;
              memmove(p, p2, SIZE_MACROS -(p2 -macros));
              break;
            }
            p = strchr(p, 0) +1;
          }

          p = macros;
          while (*p)
            p = strchr(p, 0) +1;
          if ((ssize_t)strlen(line) <= SIZE_MACROS -2 -(p - macros))
          {
            strcpy(p, line);
            *(p +strlen(line) +1) = 0;
          }
          // No space -error is not reported.
        }
      }  
      else
      if (test_header(&hlen, line, HDR_System_message, HDR_System_message2))
      {
        // 3.1.7:
        // *system_msg = yesno(cutspaces(strcpyo(line, line +hlen)));
        cutspaces(strcpyo(line, line + hlen));
        if (!strcasecmp(line, "ToSIM") || !strcmp(line, "2"))
          *system_msg = 2;
        else
          *system_msg = yesno(line);
      }
      else
      if (test_header(&hlen, line, HDR_Smsd_debug, HDR_Smsd_debug2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_SMSD_DEBUG)
          strcpy(smsd_debug, line); // smsd_debug is global.
      }
      else // if the header is unknown, then simply ignore
      {
        ;;
      }
    }    // End of header reached
    fclose(File);
#ifdef DEBUGMSG
  printf("!! to=%s\n",to);
  printf("!! from=%s\n",from);
  printf("!! alphabet=%i\n",*alphabet);
  printf("!! with_udh=%i\n",*with_udh);
  printf("!! udh_data=%s\n",udh_data);
  printf("!! queue=%s\n",queue);
  printf("!! flash=%i\n",*flash);
  printf("!! smsc=%s\n",smsc);
  printf("!! report=%i\n",*report);
  printf("!! split=%i\n",*split);
  printf("!! validity=%i\n",*validity);
  if (recursion_level == 0 && macros)
  {
    char *p = macros;

    printf("!! %s\n", (*p)? "macros:" : "no macros");
    while (*p)
    {
      printf ("%s\n", p);
      p = strchr(p, 0) +1;
    }
  }
#endif 
  }
  else
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.",filename));
    alarm_handler0(LOG_ERR, tb);
  }
}


/* =======================================================================
   Read the message text or binary data of an SMS file
   ======================================================================= */

void readSMStext(char* filename, /* Filename */
                 int recursion_level,
                 int do_convert, /* shall I convert from ISO to GSM? Do not try to convert binary data. */
// output variables are:
                 char* text,     /* message text */
                 int* textlen,   /* text length */
                 char *macros)
{
  FILE *fp;
  int in_headers = 1;
  char line[MAXTEXT +1]; // 3.1beta7: We now need a 0-termination for extract_macros.
  int n;
  int hlen;

  if (recursion_level == 0)
  {
    // Initialize result with empty string
    text[0]=0;
    *textlen=0;
  }

#ifdef DEBUGMSG
  printf("readSMStext(filename=%s, recursion_level=%i, do_convert=%i, ...)\n",filename, recursion_level, do_convert);
#endif
  
  if (!(fp = fopen(filename, "r")))
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.",filename));
    alarm_handler0(LOG_ERR, tb);
  }
  else
  {
    while (in_headers && fgets(line, sizeof(line), fp))
    {
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        strcpyo(line, line +hlen);
        cutspaces(line);
        if (recursion_level < 1)
          readSMStext(line, recursion_level +1, do_convert, text, textlen, macros);
      }

      if (line_is_blank(line))
        in_headers = 0;
    }

    n = fread(line, 1, sizeof(line) -1, fp);
    fclose(fp);

    if (n > 0)
    {
      // Convert character set or simply copy 
      if (do_convert == 1)
      {
        if (macros && *macros)
        {
          line[n] = 0;
          extract_macros(line, sizeof(line), macros);
          n = strlen(line);
        }

        // 3.1.7:
        if (trim_text)
        {
          // 3.1.9: do not trim if it becomes empty:
          char *saved_line;
          int saved_n;

          line[n] = 0;
          saved_line = strdup(line);
          saved_n = n;

          while (n > 0)
          {
            if (line[n - 1] && strchr(" \t\n\r", line[n - 1]))
            {
              n--;
              line[n] = 0;
            }
            else
              break;
          }

          if (!(*line))
          {
            strcpy(line, saved_line);
            n = saved_n;
          }
          free(saved_line);
        }

        *textlen += iso_utf8_2gsm(line, n, text + *textlen, MAXTEXT - *textlen);
      }
      else
      {
        memmove(text + *textlen, line, n);
        *textlen += n;
      }
    }
  }

#ifdef DEBUGMSG
  printf("!! textlen=%i\n",*textlen);
#endif
}

void readSMShex(char *filename, int recursion_level, char *text, int *textlen, char *macros, char *errortext)
{
  FILE *fp;
  char line[MAXTEXT +1];
  int in_headers = 1;
  char *p;
  int h;
  int i = 0;
  char *p_length = NULL;
  int too_long = 0;
  int hlen;

  if (recursion_level == 0)
  {
    text[0]=0;
    *textlen=0;
  }

  if ((fp = fopen(filename, "r")))
  {
    while (in_headers && fgets(line, sizeof(line), fp))
    {
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        strcpyo(line, line +hlen);
        cutspaces(line);
        if (recursion_level < 1)
          readSMShex(line, recursion_level +1, text, textlen, macros, errortext);
      }

      if (line_is_blank(line))
        in_headers = 0;
    }

    while (fgets(line, sizeof(line), fp) && !too_long)
    {
      cut_ctrl(line);
      while (*line == ' ' || *line == '\t')
        strcpyo(line, line +1);

      extract_macros(line, sizeof(line), macros);

      if (strncmp(line, "INLINESTRING:", 13) == 0)
      {
        if (*textlen +strlen(line) +1 -13 +1 >= MAXTEXT)
        {
          too_long = 1;
          break;
        }
        // Inline String follows:
        text[*textlen] = 0x03;
        (*textlen)++;
        // Actual text:
        strcpy(text + *textlen, line +13);
        *textlen += strlen(line) -13;
        // Termination:
        text[*textlen] = 0x00;
        (*textlen)++;
      }
      else
      if (strncmp(line, "STRING:", 7) == 0)
      {
        if (*textlen +strlen(line) -7 >= MAXTEXT)
        {
          too_long = 1;
          break;
        }
        strcpy(text + *textlen, line +7);
        *textlen += strlen(line) -7;
      }
      else
      if (strncmp(line, "LENGTH", 6) == 0)
      {
        if (!p_length)
        {
          if (*textlen +1 >= MAXTEXT)
          {
            too_long = 1;
            break;
          }
          p_length = text + *textlen;
          (*textlen)++;
        }
        else
        {
          *p_length = text + *textlen - p_length -1;
          p_length = NULL;
        }
      }
      else
      {
        if ((p = strstr(line, "/")))
          *p = 0;
        if ((p = strstr(line, "'")))
          *p = 0;
        if ((p = strstr(line, "#")))
          *p = 0;
        if ((p = strstr(line, ":")))
          *p = 0;
        while ((p = strchr(line, ' ')))
          strcpyo(p, p +1);

        if (*line)
        {
          if (strlen(line) % 2 != 0)
          {
            writelogfile0(LOG_ERR, 1, tb_sprintf("Hex presentation error in sms file %s: incorrect length of data: \"%s\".", filename, line));
            alarm_handler0(LOG_ERR, tb);
            if (errortext)
              strcpy(errortext, tb);
            text[0]=0;
            *textlen=0;
            break;
          }

          p = line;
          while (*p)
          {
            if ((i = sscanf(p, "%2x", &h)) == 0)
              break;
            if (*textlen +1 >= MAXTEXT)
            {
              too_long = 1;
              break; // Main loop is breaked by too_long variable.
            }
            text[*textlen] = h;
            (*textlen)++;
            p += 2;
          }

          if (i < 1)
          {
            writelogfile0(LOG_ERR, 1, tb_sprintf("Hex conversion error in sms file %s: \"%s\".", filename, p));
            alarm_handler0(LOG_ERR, tb);
            if (errortext)
              strcpy(errortext, tb);
            text[0]=0;
            *textlen=0;
            break;
          }
        }
      }
    }

    fclose(fp);

    if (p_length)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("LENGTH termination error in sms file %s.", filename));
      alarm_handler0(LOG_ERR, tb);
      if (errortext)
        strcpy(errortext, tb);
      text[0]=0;
      *textlen=0;
    }

    if (too_long)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("Data is too long in sms file %s.", filename));
      alarm_handler0(LOG_ERR, tb);
      if (errortext)
        strcpy(errortext, tb);
      text[0]=0;
      *textlen=0;
    }
  }
  else
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.", filename));
    alarm_handler0(LOG_ERR, tb);
    if (errortext)
      strcpy(errortext, tb);
  }

  // No need to show filename in the message file:
  if (errortext)
    if ((p = strstr(errortext, filename)))
      strcpyo(p -1, p +strlen(filename));
}

/* =======================================================================
   Mainspooler (sorts SMS into queues)
   ======================================================================= */

void mainspooler()
{
  char filename[PATH_MAX];
  char newfilename[PATH_MAX];
  char to[SIZE_TO];
  char from[SIZE_FROM];
  char smsc[SIZE_SMSC];
  char queuename[SIZE_QUEUENAME];
  char directory[PATH_MAX];
  char cmdline[PATH_MAX+PATH_MAX+32];
  int with_udh;
  char udh_data[SIZE_UDH_DATA];
  int queue;
  int alphabet;
  int i;
  int flash;
  int success;
  int report;
  int split;
  int validity;
  int voicecall;
  int hex;
  int replace_msg;
  int system_msg;
  int to_type;
  time_t now;
  time_t last_regular_run;
  time_t last_stats;
  time_t last_status;
  time_t last_spooling;
  char *fail_text = 0;
  char text[MAXTEXT];
  int textlen;

  *smsd_debug = 0;

  // 3.1.6: mainprocess will mark starting time to the trouble log and after it "Everything ok now.":
  flush_smart_logging();
  writelogfile(LOG_NOTICE, 1, "Outgoing file checker has started. PID: %i.", (int)getpid());
  flush_smart_logging();

  // 3.1.12:
  sprintf(cmdline, "All PID's: %i", (int)getpid());
  for (i = 0; i < NUMBER_OF_MODEMS; i++)
    if (device_pids[i] > 0)
      sprintf(strchr(cmdline, 0), ",%i", (int)device_pids[i]);
  writelogfile0(LOG_DEBUG, 0, cmdline);

  last_regular_run = 0; // time(0);
  last_stats = time(0);
  last_status = time(0);
  last_spooling = 0;
  workless_delay = 1;
  break_workless_delay = 0;

  // 3.1.4:
  if (delaytime_mainprocess == -1)
    delaytime_mainprocess = delaytime;

  flush_smart_logging();

  while (terminate == 0)
  {
    now = time(0);
    if (now >= last_stats +1)
    {
      last_stats = now;
      print_status();
      checkwritestats();
    }

    // 3.1.5:
    if (now >= last_status +status_interval)
    {
      last_status = now;
      write_status();
    }

    if (terminate == 1)
      return;

    if (*regular_run && regular_run_interval > 0)
    {
      now = time(0);
      if (now >= last_regular_run +regular_run_interval)
      {
        last_regular_run = now;
        writelogfile(LOG_INFO, 0, "Running a regular_run.");
        i = exec_system(regular_run, EXEC_RR_MAINPROCESS);
        if (i)
          writelogfile(LOG_ERR, 1, "Regular_run returned %i.", i);
      }
    }

    if (terminate == 1)
      return;

    now = time(0);
    if ((now >= last_spooling + delaytime_mainprocess) || break_workless_delay)
    {
      // In Cygwin serial port cannot be opened if another smsd is started.
      // This causes another modem process to shut down. Therefore pid checking is not necessary.
      // 3.1.14: Still do the test in Cygwin too, because two mainspoolers may cause "conflicts":
      //if (!os_cygwin)
      //{
        if ((i = check_pid(pidfile)))
        {
          writelogfile(LOG_ERR, 1, tb_sprintf("FATAL ERROR: Looks like another smsd (%i) is running. I (%i) quit now.", i, (int)getpid()));
          alarm_handler(LOG_ERR, tb);
          *pidfile = 0;
          abnormal_termination(1);
        }
      //}

      last_spooling = now;
      workless_delay = 0;
      break_workless_delay = 0;
      success=0;
      *tb = 0;

      if (getfile(trust_outgoing, d_spool, filename, 0))
      {
        // Checkhandler is run first, it can make changes to the message file:
        // Does the checkhandler accept the message?
        i = run_checkhandler(filename);
        if (i == 1)
        {   
          writelogfile(LOG_NOTICE, 0, tb_sprintf("SMS file %s rejected by checkhandler", filename));
          alarm_handler(LOG_NOTICE, tb);    
          fail_text = "Rejected by checkhandler";
        }
        else
        // Did checkhandler move the file by itself?
        if (i == 2)
        {   
          writelogfile(LOG_NOTICE, 0, "SMS file %s spooled by checkhandler",filename);
          success = 1;
        }
        else
        {
          // If checkhandler failed, message is processed as usual.

          readSMSheader(filename, 0, to,from,&alphabet,&with_udh,udh_data,queuename,&flash,smsc,&report,&split,
                        &validity, &voicecall, &hex, &replace_msg, NULL, &system_msg, &to_type);
          readSMStext(filename, 0, 0,text, &textlen, NULL);

          // Is the destination set?
          if (to[0]==0)
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("No destination in file %s", filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "No destination";
          }
          // is To: in the blacklist?
          else if (inblacklist(to))
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("Destination %s in file %s is blacklisted", to, filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "Destination is blacklisted";
          }
          // Whitelist can also set the queue.
          // is To: in the whitelist?
          else if (!inwhitelist_q(to, queuename))
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("Destination %s in file %s is not whitelisted", to, filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "Destination is not whitelisted";
          }
          // Is the alphabet setting valid?
#ifdef USE_ICONV
          else if (alphabet>3)
#else
          else if (alphabet>2)
#endif
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("Invalid alphabet in file %s", filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "Invalid alphabet";
          }
          // is there is a queue name, then set the queue by this name
          else if ((queuename[0]) && ((queue=getqueue(queuename,directory))==-1))
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("Wrong provider queue %s in file %s", queuename, filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "Wrong provider queue";
          }
          // if there is no queue name, set it by the destination phone number
          else if ((queuename[0]==0) && ((queue=getqueue(to,directory))==-1))
          {
            writelogfile(LOG_NOTICE, 0, tb_sprintf("Destination number %s in file %s does not match any provider", to, filename));
            alarm_handler(LOG_NOTICE, tb);
            fail_text = "Destination number does not match any provider";
          }
          // If a file has no text or data, it can be rejected here.
          else if (textlen <= 0)
          {
            writelogfile0(LOG_NOTICE, 0, tb_sprintf("The file %s has no text or data",filename));
            alarm_handler0(LOG_NOTICE, tb);
            fail_text = "No text or data";
          }
          // Is numbering plan set correctly
          else if (to_type == -2)
          {
            writelogfile0(LOG_NOTICE, 0, tb_sprintf("Invalid number type in file %s", filename));
            alarm_handler0(LOG_NOTICE, tb);
            fail_text = "Invalid number type";
          }
          else
          {
            // Everything is ok, move the file into the queue

            // 3.1beta7, 3.0.9: Return value handled:
            if (movefilewithdestlock_new(filename, directory, keep_filename, store_original_filename, "", newfilename) == 1)
            {
              // 1 = lockfile cannot be created. It exists.
              writelogfile0(LOG_CRIT, 1, tb_sprintf("Conflict with .LOCK file in the spooler: %s %s", filename, directory));
              alarm_handler0(LOG_CRIT, tb);

              // TODO: If original filename was used, could switch to unique mode automatically.

              if (movefilewithdestlock_new(filename, directory, keep_filename, store_original_filename, "", newfilename) == 0)
                writelogfile0(LOG_CRIT, 0, tb_sprintf("Retry solved the spooler conflict: %s %s", filename, directory));
            }

            stop_if_file_exists("Cannot move", filename, "to", directory);
            writelogfile(LOG_NOTICE, 0, "Moved file %s to %s", filename, (keep_filename)? directory : newfilename);
            success = 1;
            sendsignal2devices(SIGCONT);
          }
        }

        if (!success)
        {
          char remove_headers[4096];
          char add_headers[4096];
          // 3.1.1: Empty file is not moved to the failed folder.
          struct stat statbuf;
          int file_is_empty = 0;
          char timestamp[81];

          if (stat(filename, &statbuf) == 0)
            if (statbuf.st_size == 0)
              file_is_empty = 1;

          prepare_remove_headers(remove_headers, sizeof(remove_headers));
          *add_headers = 0;
          if (fail_text && *fail_text && *HDR_FailReason2 != '-')
            sprintf(add_headers, "%s %s\n", get_header(HDR_FailReason, HDR_FailReason2), fail_text);

          // 3.1.5: Timestamp for failed message:
          make_datetime_string(timestamp, sizeof(timestamp), 0, 0, 0);
          if (*HDR_Failed2 != '-')
            sprintf(strchr(add_headers, 0), "%s %s\n",
                    get_header(HDR_Failed, HDR_Failed2), timestamp);

          change_headers(filename, remove_headers, 0, "%s", add_headers);

          rejected_counter++;

          if (d_failed[0] && !file_is_empty)
          {
            movefilewithdestlock_new(filename,d_failed, keep_filename, store_original_filename, process_title, newfilename);
            stop_if_file_exists("Cannot move",filename,"to",d_failed);
          }
        
          if (eventhandler[0])
          {
            snprintf(cmdline, sizeof(cmdline), "%s %s %s", eventhandler, "FAILED", (d_failed[0] == 0 || file_is_empty)? filename : newfilename);
            exec_system(cmdline, EXEC_EVENTHANDLER);
          }

          if (d_failed[0] == 0 || file_is_empty)
          {
            unlink(filename);
            stop_if_file_exists("Cannot delete",filename,"","");
            writelogfile(LOG_INFO, 0, "Deleted file %s",filename);
          }
        }

        last_spooling = 0;
        continue;
      }

      workless_delay = 1;
    }

    if (terminate == 1)
      return;

    flush_smart_logging();

    // 3.1.7:
    //sleep(1);
    if (delaytime_mainprocess != 0 && !terminate && !break_workless_delay)
      t_sleep(1);

  }
}

/* =======================================================================
   Delete message on the SIM card
   ======================================================================= */

void deletesms(int sim) /* deletes the selected sms from the sim card */
{
  char command[100];
  char answer[500];

  if (sim == PDUFROMFILE)
    return;

  if (keep_messages || DEVICE.keep_messages)
    writelogfile(LOG_INFO, 0, "Keeping message %i", sim);
  else
  {
    writelogfile(LOG_INFO, 0, "Deleting message %i", sim);
    sprintf(command,"AT+CMGD=%i\r", sim);
    put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
  }
}

void deletesms_list(char *memory_list)
{
  char command[100];
  char answer[500];
  int sim;
  char *p;

  while (*memory_list)
  {
    sim = atoi(memory_list);

    if ((p = strchr(memory_list, ',')))
      strcpyo(memory_list, p +1);
    else
      *memory_list = 0;

    if (sim != PDUFROMFILE)
    {
      if (keep_messages || DEVICE.keep_messages)
        writelogfile(LOG_INFO, 0, "Keeping message %i", sim);
      else
      {
        writelogfile(LOG_INFO, 0, "Deleting message %i", sim);
        sprintf(command,"AT+CMGD=%i\r", sim);
        put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
      }
    }
  }
}

/* =======================================================================
   Check size of SIM card
   ======================================================================= */

// Return value: 1 = OK, 0 = check failed.
int check_memory(int *used_memory, int *max_memory, char *memory_list, size_t memory_list_size,
                 char *delete_list, int delete_list_size)
{
  // 3.1.5: GMGL list needs much more space: using global buffer.
  //char answer[500];
  char *answer = check_memory_buffer;
  // 3.1.12: Use allocated memory:
  //int size_answer = SIZE_CHECK_MEMORY_BUFFER;
  int size_answer = (int)check_memory_buffer_size;

  char* start;
  char* end;
  char tmp[100];
  char *p;
  char *pos;
  int i;

  if (!answer)
    return 0;

  (void) delete_list_size;        // 3.1.7: remove warning.
  // Set default values in case that the modem does not support the +CPMS command
  *used_memory=1;
  *max_memory=10;

  *answer = 0;

  // Ability to read incoming PDU from file:
  if (DEVICE.pdu_from_file[0])
  {
    FILE *fp;
    char filename[PATH_MAX];

    strcpy(filename, DEVICE.pdu_from_file);
    if (getpdufile(filename))
    {
      if ((fp = fopen(filename, "r")))
      {
        fclose(fp);
        writelogfile(LOG_INFO, 0, "Found an incoming message file %s", filename);
        return 1;
      }
    }
  }

  if (DEVICE.modem_disabled == 1)
  {
    *used_memory = 0;
    return 1;
  }

  writelogfile(LOG_INFO, 0, "Checking memory size");

  // 3.1.5:
  if (DEVICE.check_memory_method == CM_CMGD)
  {
    *used_memory = 0;
    *memory_list = 0;
    put_command("AT+CMGD=?\r", answer, size_answer, 1, "(\\+CMGD:.*OK)|(ERROR)");
    // +CMGD: (1,22,23,28),(0-4) \r OK
    // +CMGD: (),(0-4) \r OK

    if (strstr(answer, "()"))
      return 1;

    if ((p = strstr(answer, " (")))
    {
      strcpyo(answer, p +2);
      if ((p = strstr(answer, "),")))
      {
        *p = 0;
        if (strlen(answer) < memory_list_size)
        {
          *used_memory = 1;
          strcpy(memory_list, answer);
          p = memory_list;
          while ((*p) && (p = strstr(p +1, ",")))
            (*used_memory)++;
        }
      }
      else
        writelogfile(LOG_INFO, 1, "Incomplete answer for AT+CMGD=?, feature not supported?");
    }
    writelogfile(LOG_INFO, 0, "Used memory is %i%s%s", *used_memory, (*memory_list)? ", list: " : "", memory_list);
    return 1;
  }
  else
  if (value_in(DEVICE.check_memory_method, 5, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
  {
    // 3.1.5: Check CMGL result, it can be incorrect or broken if bad baudrate is used:
    char *errorstr = 0;
    char *term;
    char *p2;
    int mnumber;
    int mlength;
    int pdu1;
    int save_log_single_lines = log_single_lines;
    char buffer[256 *LENGTH_PDU_DETAIL_REC +1];

    *used_memory = 0;
    *memory_list = 0;
    *buffer = 0;

    sprintf(tmp, "AT+CMGL=%s\r", DEVICE.cmgl_value);

    // 3.1.5: Empty list gives OK answer without +CMGL prefix:
    log_single_lines = 0;

    // 3.1.12: With large number of messages and slow modem, much longer timeout than "1" is required.
    put_command(tmp, answer, size_answer, 10, EXPECT_OK_ERROR);

    log_single_lines = save_log_single_lines;

    pos = answer;
    while ((p = strstr(pos, "+CMGL:")))
    {
      mnumber = 0; // initial value for error message
      if (!(term = strchr(p, '\r')))
      {
        errorstr = "Line end not found (fields)";
        break;
      }

      // 3.1.6: Message number can be zero:
      //if ((mnumber = atoi(p + 6)) <= 0)
      mnumber = (int) strtol(p +6, NULL, 0);
      if (errno == EINVAL || mnumber < 0)
      {
        errorstr = "Invalid message number";
        break;
      }

      if (value_in(DEVICE.check_memory_method, 3, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
      {
        p2 = term;
        while (*p2 != ',')
        {
          if (p2 <= p)
          {
            errorstr = "Message length field not found";
            break;
          }
          p2--;
        }

        if (errorstr)
          break;
       
        if ((mlength = atoi(p2 +1)) <= 0)
        {
          errorstr = "Invalid length information";
          break;
        }

        p = term +1;
        if (*p == '\n')
          p++;
        if ((pdu1 = octet2bin_check(p)) <= 0)
        {
          errorstr = "Invalid first byte of PDU";
          break;
        }

        if (!(term = strchr(p, '\r')))
        {
          errorstr = "Line end not found (PDU)";
          break;
        }

        // Some devices give PDU total length, some give length of TPDU:
        if (pdu1 *2 +2 +mlength *2 != (int)(term -p) &&
            mlength *2 != (int)(term -p))
        {
          errorstr = "PDU does not match with length information";
          break;
        }

        i =  get_pdu_details(buffer, sizeof(buffer), p, mnumber);
        if (i > 1)
        {
          errorstr = "Fatal error";
          break;
        }
      }

      // By default this is the result:
      sprintf(tmp, "%s%i", (*memory_list)? "," : "", mnumber);
      if (strlen(memory_list) +strlen(tmp) < memory_list_size)
      {
        (*used_memory)++;
        strcat(memory_list, tmp);
      }
      else
      {
        errorstr = "Too many messages";
        break;
      }

      pos = term +1;
    }

    if (errorstr)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("CMGL handling error: message %i, %s", mnumber, errorstr));
      alarm_handler0(LOG_ERR, tb);

      return 0;
    }

    if (*buffer)
    {
      sort_pdu_details(buffer);

      // Show even if there is only one message, timestamp may be interesting.
      //if (strlen(buffer) > LENGTH_PDU_DETAIL_REC)
        if (strstr(smsd_version, "beta"))
          writelogfile(LOG_DEBUG, 0, "Sorted message order:\n%s", buffer);
    }

    // If there is at least one message and SIMCOM SIM600 style handling is required.
    // ( one because delete_list is always needed ).
    if (*used_memory > 0 && value_in(DEVICE.check_memory_method, 1, CM_CMGL_SIMCOM))
    {
/*
001 A 358401234567____________________111 001/001 r 00-00-00 00-00-00n

3.1.7:
001 A 00-00-00 00-00-00 358401234567____________________111 001/001 rn

*/
      int pos_sender = 24;    //6;
      int pos_messageid = 56; //38;
      int length_messagedetail = 11;
      int pos_partnr = 60;    //42;
      int pos_partcount = 64; //46;
      int pos_timestamp = 6;  //52;
      int offset_messageid = 32;      // from sender_id

      // NOTE:
      // Exact size for buffers:
      char sender[32 +1];
      char sender_id[35 +1];

      int p_count;
      int read_all_parts;

      *used_memory = 0;
      *memory_list = 0;
      *delete_list = 0;
      pos = buffer;
      while (*pos)
      {
        if (!strncmp(&pos[pos_messageid], "999 001/001", length_messagedetail))
        {
          // Single part message. Always taken and fits to the list.
          mnumber = atoi(pos);

          sprintf(strchr(memory_list, 0), "%s%i", (*memory_list)? "," : "", mnumber);
          (*used_memory)++;

          sprintf(strchr(delete_list, 0), "%s%i", (*delete_list)? "," : "", mnumber);

          pos += LENGTH_PDU_DETAIL_REC;
          continue;
        }

        // Multipart message, first part which is seen.
        strncpy(sender, &pos[pos_sender], sizeof(sender) -1);
        sender[sizeof(sender) -1] = 0;
        while (sender[strlen(sender) -1] == ' ')
          sender[strlen(sender) -1] = 0;

        strncpy(sender_id, &pos[pos_sender], sizeof(sender_id) -1);
        sender_id[sizeof(sender_id) -1] = 0;

        p_count = atoi(&pos[pos_partcount]);
        p = pos;
        for (i = 1; *p && i <= p_count; i++)
        {
          if (strncmp(&p[pos_sender], sender_id, sizeof(sender_id) -1) ||
              atoi(&p[pos_partnr]) != i)
            break;
          p += LENGTH_PDU_DETAIL_REC;
        }

        read_all_parts = 1;

        if (i <= p_count)
        {
          // Some part(s) missing.
          // With SIM600: only the first _available_ part can be deleted and all
          // other parts of message are deleted by the modem.
          int ms_purge;
          int remaining = -1;

          read_all_parts = 0;

          if ((ms_purge = DEVICE.ms_purge_hours *60 +DEVICE.ms_purge_minutes) > 0)
          {
            time_t rawtime;
            struct tm *timeinfo;
            time_t now;
            time_t msgtime;

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            now = mktime(timeinfo);

            p = pos +pos_timestamp;
            timeinfo->tm_year = atoi(p) +100;
            timeinfo->tm_mon = atoi(p +3) -1;
            timeinfo->tm_mday = atoi(p +6);
            timeinfo->tm_hour = atoi(p +9);
            timeinfo->tm_min = atoi(p +12);
            timeinfo->tm_sec = atoi(p +15);
            msgtime = mktime(timeinfo);

            if (ms_purge *60 > now - msgtime)
            {
              remaining = ms_purge - (now - msgtime) /60;
              ms_purge = 0;
            }
          }

          if (ms_purge > 0)
          {
            if (DEVICE.ms_purge_read)
            {
              read_all_parts = 1;
              writelogfile0(LOG_NOTICE, 0, tb_sprintf("Reading message %s from %s partially, all parts not found and timeout expired",
                            sender_id +offset_messageid, sender));
            }
            else
            {
              writelogfile0(LOG_NOTICE, 0, tb_sprintf("Deleting message %s from %s, all parts not found and timeout expired",
                            sender_id +offset_messageid, sender));
              deletesms(atoi(pos));
            }
          }
          else
          {
            writelogfile0(LOG_INFO, 0, tb_sprintf("Skipping message %s from %s, all parts not found", sender_id +offset_messageid, sender));
            if (remaining > 0)
              writelogfile0(LOG_DEBUG, 0, tb_sprintf("Message %s from %s will be %s after %i minutes unless remaining parts are received",
                            sender_id +offset_messageid, sender, (DEVICE.ms_purge_read)? "read partially" : "deleted", remaining));
          }
        }

        if (read_all_parts)
          sprintf(strchr(delete_list, 0), "%s%i", (*delete_list)? "," : "", atoi(pos));

        while (*pos)
        {
          if (strncmp(&pos[pos_sender], sender_id, sizeof(sender_id) -1))
            break;

          if (read_all_parts)
          {
            sprintf(strchr(memory_list, 0), "%s%i", (*memory_list)? "," : "", atoi(pos));
            (*used_memory)++;
          }

          pos += LENGTH_PDU_DETAIL_REC;
        }
      }
    }
    else
    if (*buffer)
    {
      // Re-create memory_list because messages are sorted.
      *used_memory = 0;
      *memory_list = 0;

      pos = buffer;
      while (*pos)
      {
        sprintf(strchr(memory_list, 0), "%s%i", (*memory_list)? "," : "", atoi(pos));
        (*used_memory)++;
        pos += LENGTH_PDU_DETAIL_REC;
      }
    }

    writelogfile(LOG_INFO, 0, "Used memory is %i%s%s", *used_memory, (*memory_list)? ", list: " : "", memory_list);

    if (*delete_list && value_in(DEVICE.check_memory_method, 1, CM_CMGL_SIMCOM))
      writelogfile(LOG_DEBUG, 0, "Will later delete messages: %s", delete_list);

    return 1;
  }
  else
  {
    put_command("AT+CPMS?\r", answer, size_answer, 1, "(\\+CPMS:.*OK)|(ERROR)");
    if ((start=strstr(answer,"+CPMS:")))
    {
      // 3.1.5: Check if reading of messages is not supported:
      if (strstr(answer, "+CPMS: ,,,,,,,,"))
        writelogfile(LOG_INFO, 1, "Reading of messages is not supported?");
      else
      {
        end=strchr(start,'\r');
        if (end)
        {
          *end=0;
          getfield(start,2,tmp, sizeof(tmp));
          if (tmp[0])
            *used_memory=atoi(tmp);
          getfield(start,3,tmp, sizeof(tmp));
          if (tmp[0])
            *max_memory=atoi(tmp);    
          writelogfile(LOG_INFO, 0, "Used memory is %i of %i",*used_memory,*max_memory);
          return 1;
        }
      }
    }
  }

  // 3.1.5: If CPMS did not work but it should work:
  if (DEVICE.check_memory_method == CM_CPMS)
  {
    writelogfile(LOG_INFO, 1, "Command failed.");
    return 0;
  }
  
  writelogfile(LOG_INFO, 0, "Command failed, using defaults.");
  return 1;
}


/* =======================================================================
   Read and delete phonecall (phonebook) entries
   Return value:
   -2 = fatal error while handling an answer
   -1 = modem does not support necessary commands
   0 = no entries
   > 0 = number of entries processed
   ======================================================================= */

int savephonecall(char *entry_number, int entry_type, char *entry_text)
{
	int result = 0;
	char filename[PATH_MAX];
	FILE *fp;
	char timestamp[81];
	char cmdline[PATH_MAX+PATH_MAX+32];
	char e_type[SIZE_PB_ENTRY];

	make_datetime_string(timestamp, sizeof(timestamp), 0, 0, date_filename_format);
	if (date_filename == 1)
		sprintf(filename, "%s/%s.%s.XXXXXX", (*d_phonecalls)? d_phonecalls : d_incoming, timestamp, DEVICE.name);
	else if (date_filename == 2)
		sprintf(filename, "%s/%s.%s.XXXXXX", (*d_phonecalls)? d_phonecalls : d_incoming, DEVICE.name, timestamp);
	else
		sprintf(filename, "%s/%s.XXXXXX", (*d_phonecalls)? d_phonecalls : d_incoming, DEVICE.name);

	close(mkstemp(filename));
	unlink(filename);
	if (!(fp = fopen(filename, "w")))
	{
		writelogfile0(LOG_ERR, 1, tb_sprintf("Could not create file %s for phonecall from %s: %s", filename, entry_number, strerror(errno)));
		alarm_handler0(LOG_ERR, tb);
		result = 1;
	}
	else
	{
		char *message_body;

		explain_toa(e_type, 0, entry_type);

		fprintf(fp, "%s %s\n", get_header_incoming(HDR_From, HDR_From2), entry_number);

		if (*HDR_FromTOA2 != '-')
			fprintf(fp, "%s %s\n", get_header_incoming(HDR_FromTOA, HDR_FromTOA2), e_type);

		if (entry_text[0] && *HDR_Name2 != '-')
			fprintf(fp,"%s %s\n", get_header_incoming(HDR_Name, HDR_Name2), entry_text);

		if (*HDR_Subject2 != '-')
			fprintf(fp, "%s %s\n", get_header_incoming(HDR_Subject, HDR_Subject2), DEVICE.name);

		if (*HDR_Modem2 != '-')
			fprintf(fp, "%s %s\n", get_header_incoming(HDR_Modem, HDR_Modem2), DEVICE.name);

		if (DEVICE.number[0])
			if (*HDR_Number2 != '-')
				fprintf(fp, "%s %s\n", get_header_incoming(HDR_Number, HDR_Number2), DEVICE.number);

		if (!strstr(DEVICE.identity, "ERROR") && *HDR_Identity2 != '-')
			fprintf(fp, "%s %s\n", get_header_incoming(HDR_Identity, HDR_Identity2), DEVICE.identity);

		if (*HDR_CallType2 != '-')
			fprintf(fp, "%s %s\n",
				get_header_incoming(HDR_CallType, HDR_CallType2),
				get_header_incoming(HDR_missed, HDR_missed2));

		make_datetime_string(timestamp, sizeof(timestamp), 0, 0, 0);
		fprintf(fp, "%s %s\n", get_header_incoming(HDR_Received, HDR_Received2), timestamp);

		message_body = get_header_incoming(HDR_missed_text, HDR_missed_text2);
		fprintf(fp, "\n%s\n", message_body);

		fclose(fp);

		apply_filename_preview(filename, message_body, filename_preview);                          
		writelogfile(LOG_INFO, 0, "Wrote an incoming message file: %s", filename);

		if (eventhandler[0] || DEVICE.eventhandler[0])
		{
			snprintf(cmdline, sizeof(cmdline), "%s %s %s", (DEVICE.eventhandler[0])? DEVICE.eventhandler : eventhandler, "CALL", filename);
			exec_system(cmdline, EXEC_EVENTHANDLER);
		}
	}

	return result;
}

int readphonecalls()
{
  int result = 0;
  char command[1024];
  char answer[2048];
  static int errors = 0;
  #define PB_MAX_ERRORS DEVICE.phonecalls_error_max
  static int index_max = 0;
  #define PB_INDEX_DEFAULT 0
  char *p, *p2, *e_start, *e_end, *e_line_end;
  int len;
  int count, ok;
  char entry_number[SIZE_PB_ENTRY];
  char entry_type[SIZE_PB_ENTRY];
  char entry_text[SIZE_PB_ENTRY];

  // THIS FUNCTION CAN RETURN DIRECTLY (if smsd is terminating).

  if (errors >= PB_MAX_ERRORS)
    result = -1;
  else
  {
    writelogfile(LOG_INFO, 0, "Reading phonecall entries");

    sprintf(command,"AT+CPBS=\"%s\"\r", "MC");
    put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
    if (strstr(answer, "ERROR"))
    {
      if (++errors >= PB_MAX_ERRORS)
      {
        writelogfile(LOG_INFO, 1, "Ignoring phonecalls, too many errors");
        result = -1;
      }
    }
    else
    {
      if (index_max == 0)
      {
        if (terminate == 1)
          return 0;

        writelogfile(LOG_INFO, 0, "Checking phonecall limits (once)");
        sprintf(command,"AT+CPBR=?\r");
        put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
        if (strstr(answer, "ERROR"))
        {
          if (++errors >= PB_MAX_ERRORS)
          {
            writelogfile(LOG_INFO, 1, "Ignoring phonecalls, too many errors");
            result = -1;
          }
        }
        else
        {
          if ((p = strchr(answer, '-')))
          {
            p++;
            if ((p2 = strchr(p, ')')))
              *p2 = 0;
            index_max = atoi(p);
            writelogfile(LOG_INFO, 0, "Phonecall limit is %i", index_max);
          }
          else
            index_max = PB_INDEX_DEFAULT;
        }
      }

      if (index_max <= 0)
      {
        errors = PB_MAX_ERRORS;
        writelogfile(LOG_INFO, 1, "Ignoring phonecalls, cannot resolve maximum index value");
        result = -1;
      }
      else
      {
        if (terminate == 1)
          return 0;

        sprintf(command,"AT+CPBR=1,%i\r", index_max);
        put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
        if (strstr(answer, "ERROR"))
        {
          if (++errors >= PB_MAX_ERRORS)
          {
            writelogfile(LOG_INFO, 1, "Ignoring phonecalls, too many errors");
            result = -1;
          }
        }
        else
        {
          if (!strstr(answer, "+CPBR:"))
            writelogfile(LOG_INFO, 0, "No phonecall entries");

          if (terminate == 1)
            return 0;

          // After this point terminate is not checked, because if entries are
          // processed, they should also be deleted from the phone.

          count = 0;
          p2 = answer;
          while ((p = strstr(p2, "+CPBR:")))
          {
            ok = 0;
            *entry_number = 0;
            *entry_type = 0;
            *entry_text = 0;
            e_line_end = p;
            while (*e_line_end != '\r' && *e_line_end != '\n')
            {
              if (*e_line_end == 0)
              {
                writelogfile(LOG_INFO, 1, "Fatal error while handling phonecall data");
                result = -2;
                break;
              }
              e_line_end++;
            }

            if (result != 0)
              break;

            e_start = strchr(p, '"');
            if (e_start && e_start < e_line_end)
            {
              e_start++;
              e_end = strchr(e_start, '"');
              if (e_end && e_end < e_line_end)
              {
                if ((len = e_end -e_start) < SIZE_PB_ENTRY)
                {
                  sprintf(entry_number, "%.*s", len, e_start);
                  cutspaces(entry_number);
                  if (*entry_number == '+')
                    strcpyo(entry_number, entry_number +1);
                }

                if (strlen(e_end) < 2)
                  e_end = 0;
                else
                {
                  e_start = e_end +2;
                  e_end = strchr(e_start, ',');
                }

                if (e_end && e_end < e_line_end)
                {
                  if ((len = e_end -e_start) < SIZE_PB_ENTRY)
                  {
                    sprintf(entry_type, "%.*s", len, e_start);
                    cutspaces(entry_type);

                    if (strlen(e_end) < 2)
                      e_end = 0;
                    else
                    {
                      e_start = e_end +2;
                      e_end = strchr(e_start, '"');
                    }

                    if (e_end && e_end < e_line_end)
                    {
                      if ((len = e_end -e_start) < SIZE_PB_ENTRY)
                      {
                        sprintf(entry_text, "%.*s", len, e_start);
                        cutspaces(entry_text);
                        writelogfile(LOG_INFO, 0, "Got phonecall entry from %s", entry_number);
                        ok = 1;

			savephonecall(entry_number, atoi(entry_type), entry_text);
                      }
                    }
                  }
                }
              }
            }

            if (!ok)
            {
              writelogfile(LOG_INFO, 1, "Syntax error while handling phonecall data");
              result = -2;
              break;
            }
            else
              count++;

            p2 = p +6;
          }

          if (result == 0 && count > 0 && !keep_messages && !DEVICE.keep_messages)
          {
            result = count;
            writelogfile(LOG_INFO, 0, "Removing processed phonecall entries", count);

            // 3.1.7:
            if (DEVICE.phonecalls_purge[0])
            {
              int i;

              i = yesno_check(DEVICE.phonecalls_purge);

              if (i != 0)
              {
                p = DEVICE.phonecalls_purge;
                if (i == 1)
                  p = "AT^SPBD=\"MC\"";

                sprintf(command, "%s\r", p);
                put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
                count = 0;

                if (strstr(answer, "ERROR"))
                {
                  if (++errors >= PB_MAX_ERRORS)
                  {
                    writelogfile(LOG_WARNING, 1, "Ignoring phonecalls, too many errors");
                    result = -1;
                  }
                }
              }
            }

            while (count && result != -1)
            {
              sprintf(command, "AT+CPBW=%i\r", count);
              put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
              count--;

              if (strstr(answer, "ERROR"))
              {
                if (++errors >= PB_MAX_ERRORS)
                {
                  writelogfile(LOG_WARNING, 1, "Ignoring phonecalls, too many errors");
                  result = -1;
                }
              }
            }

            if (result != -1)
              writelogfile(LOG_INFO, 0, "%i phonecall entries processed", result);
          }
        }
      }
    }
  }

  return result;
}

/* =======================================================================
   Read a memory space from SIM card
   ======================================================================= */

int readsim(int sim, char* line1, char* line2)  
/* reads a SMS from the given SIM-memory */
/* returns number of SIM memory if successful, otherwise -1 */
/* 3.1.5: In case of timeout return value is -2. */
/* line1 contains the first line of the modem answer */
/* line2 contains the pdu string */
{                                
  char command[500];
  char answer[1024];
  char* begin1;
  char* begin2;
  char* end1;
  char* end2;

  line2[0]=0;
  line1[0]=0;

#ifdef DEBUGMSG
  printf("!! readsim(sim=%i, ...)\n",sim);
#endif

  // Ability to read incoming PDU from file:
  if (sim == 1 && DEVICE.pdu_from_file[0])
  {
    FILE *fp;
    char *p;
    char filename[PATH_MAX];

    // 3.1beta7, 3.0.9: If a definition is a directory, first file found from there is taken.
    // Note: for safety reasons this directory definition is accepted only if it ends with slash and no dot
    // is used in any position of definition. A single file definition is accepted if a definition does
    // not end with slash and a directory using the same name does not exist.
    strcpy(filename, DEVICE.pdu_from_file);
    if (getpdufile(filename))
    {
      if ((fp = fopen(filename, "r")))
      {
        int result = PDUFROMFILE;
        char st[1024];

#ifdef DEBUGMSG
  printf("!! reading message from %s\n", filename);
#endif
        writelogfile(LOG_INFO, 0, "Reading an incoming message from file %s", filename);

        // 3.1beta7, 3.0.9:
        // First check if there is a line starting with "PDU: " or "PDU:_". If found, it's taken.
        while (fgets(st, sizeof(st), fp))
        {
          cutspaces(st);
          cut_ctrl(st);
          if (!strncmp(st, "PDU: ", 5) || !strncmp(st, "PDU:_", 5))
          {
            strcpy(line2, st +5);
            break;
          }
        }

        if (*line2 == 0)
        {
          fseek(fp, 0, SEEK_SET);
          while (fgets(st, sizeof(st), fp))
          {
            cutspaces(st);
            cut_ctrl(st);
            if (*st && *st != '#')
            {
              if (*line1 == 0)
                strcpy(line1, st);
              else if (*line2 == 0)
                strcpy(line2, st);
              else
                break;
            }
          }

          if (*line2 == 0)
          {
            // line1 is not necessary. If there is only one line, it should be the PDU string.
            strcpy(line2, line1);
            line1[0] = 0;
          }

          if ((p = strrchr(line2, ' ')))
            strcpyo(line2, p +1);

          if (*line2 == 0)
          {
            result = -1;
            writelogfile(LOG_CRIT, 0, "Syntax error in the incoming message file.");
          }
        }

        fclose(fp);
        unlink(filename);
#ifdef DEBUGMSG
  printf("!! read result:%i\n", result);
#endif
        return result;
      }
    }
  }

  if (DEVICE.modem_disabled == 1)
  {
    writelogfile(LOG_CRIT, 0, "Cannot try to get stored message %i, MODEM IS DISABLED", sim);
    return 0;
  }

  writelogfile(LOG_INFO, 0, "Trying to get stored message %i",sim);
  sprintf(command,"AT+CMGR=%i\r",sim);
  // 3.1beta3: Some modems answer OK in case of empty memory space (added "|(OK)")
  if (put_command(command, answer, sizeof(answer), 1, "(\\+CMGR:.*OK)|(ERROR)|(OK)") == -2)
    return -2;

  // 3.1.7:
  while ((begin1 = strchr(answer, '\n')))
    *begin1 = '\r';

  while ((begin1 = strstr(answer, "\r\r")))
    strcpyo(begin1, begin1 + 1);
  // ------

  if (strstr(answer,",,0\r")) // No SMS,  because Modem answered with +CMGR: 0,,0 
    return -1;
  if (strstr(answer,"ERROR")) // No SMS,  because Modem answered with ERROR 
    return -1;  
  begin1=strstr(answer,"+CMGR:");
  if (begin1==0)
    return -1;
  end1=strstr(begin1,"\r");
  if (end1==0)
    return -1;
  begin2=end1+1;
  end2=strstr(begin2+1,"\r");
  if (end2==0)
    return -1;
  strncpy(line1,begin1,end1-begin1);
  line1[end1-begin1]=0;
  strncpy(line2,begin2,end2-begin2);
  line2[end2-begin2]=0;
  cutspaces(line1);
  cut_ctrl(line1);
  cutspaces(line2);
  cut_ctrl(line2); 
  if (strlen(line2)==0)
    return -1;
#ifdef DEBUGMSG
  printf("!! line1=%s, line2=%s\n",line1,line2);
#endif
  return sim;
}

/* =======================================================================
   Write a received message into a file 
   ======================================================================= */
   
// returns 1 if this was a status report
int received2file(char *line1, char *line2, char *filename, int *stored_concatenated, int incomplete)
{
  int userdatalength;
  char ascii[MAXTEXT]= {};
  char sendr[100]= {};
  int with_udh=0;
  char udh_data[SIZE_UDH_DATA] = {};
  char udh_type[SIZE_UDH_TYPE] = {};
  char smsc[31]= {};
  char name[64]= {};
  char date[9]= {};
  char Time[9]= {};
  char warning_headers[SIZE_WARNING_HEADERS] = {};
  //char status[40]={}; not used
  int alphabet=0;
  int is_statusreport=0;
  FILE* fd;
  int do_decode_unicode_text = 0;
  int do_internal_combine = 0;
  int do_internal_combine_binary;
  int is_unsupported_pdu = 0;
  int pdu_store_length = 0;
  int result = 1;
  char from_toa[51] = {};
  int report;
  int replace;
  int flash;
  int p_count = 1;
  int p_number;
  char sent[81] = "";
  char received[81] = "";

  if (DEVICE.decode_unicode_text == 1 ||
      (DEVICE.decode_unicode_text == -1 && decode_unicode_text == 1))
    do_decode_unicode_text = 1;

  if (!incomplete)
    if (DEVICE.internal_combine == 1 ||
        (DEVICE.internal_combine == -1 && internal_combine == 1))
      do_internal_combine = 1;

  do_internal_combine_binary = do_internal_combine;
  if (do_internal_combine_binary)
    if (DEVICE.internal_combine_binary == 0 ||
        (DEVICE.internal_combine_binary == -1 && internal_combine_binary == 0))
      do_internal_combine_binary = 0;

#ifdef DEBUGMSG
  printf("!! received2file: line1=%s, line2=%s, decode_unicode_text=%i, internal_combine=%i, internal_combine_binary=%i\n",
         line1, line2, do_decode_unicode_text, do_internal_combine, do_internal_combine_binary);
#endif

  //getfield(line1,1,status, sizeof(status)); not used
  getfield(line1,2,name, sizeof(name));

  // Check if field 2 was a number instead of a name
  if (atoi(name)>0)
    name[0]=0; //Delete the name because it is missing

  userdatalength=splitpdu(line2, DEVICE.mode, &alphabet, sendr, date, Time, ascii, smsc, &with_udh, udh_data, udh_type,
                          &is_statusreport, &is_unsupported_pdu, from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);
  if (alphabet==-1 && DEVICE.cs_convert==1)
    userdatalength=gsm2iso(ascii,userdatalength,ascii,sizeof(ascii));
  else if (alphabet == 2 && do_decode_unicode_text == 1)
  {
    if (with_udh)
    {
      char *tmp;
      int m_id, p_count, p_number;

      if ((tmp = strdup(udh_data)))
      {
        if (get_remove_concatenation(tmp, &m_id, &p_count, &p_number) > 0)
        {
          if (p_count == 1 && p_number == 1)
          {
            strcpy(udh_data, tmp);
            if (!(*udh_data))
            {
              with_udh = 0;
              *udh_type = 0;
            }
            else
            {
              if (explain_udh(udh_type, udh_data) < 0)
                if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
                  sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");
            }
          }
        }
        free(tmp);
      }
    }  

#ifndef USE_ICONV
    // 3.1beta7, 3.0.9: decoding is always done:
    userdatalength = decode_ucs2(ascii, userdatalength);
    alphabet = 0;
#else
    userdatalength = iconv_ucs2utf(ascii, userdatalength, sizeof(ascii));
    alphabet = 4;
#endif
  }
  
#ifdef DEBUGMSG
  printf("!! userdatalength=%i\n",userdatalength);
  printf("!! name=%s\n",name);  
  printf("!! sendr=%s\n",sendr);  
  printf("!! date=%s\n",date);
  printf("!! Time=%s\n",Time); 
  if ((alphabet==-1 && DEVICE.cs_convert==1)||(alphabet==0)||(alphabet==4))
  printf("!! ascii=%s\n",ascii); 
  printf("!! smsc=%s\n",smsc); 
  printf("!! with_udh=%i\n",with_udh);
  printf("!! udh_data=%s\n",udh_data);   
  printf("!! udh_type=%s\n",udh_type);   
  printf("!! is_statusreport=%i\n",is_statusreport);   
  printf("!! is_unsupported_pdu=%i\n", is_unsupported_pdu);   
  printf("!! from_toa=%s\n", from_toa);   
  printf("!! report=%i\n", report);   
  printf("!! replace=%i\n", replace);   
#endif

  if (is_statusreport)
  {
    char *p;
    char id[41];
    char status[128];
    const char SR_MessageId[] = "Message_id:"; // Fixed title inside the status report body.
    const char SR_Status[] = "Status:"; // Fixed title inside the status report body.

    *id = 0;
    *status = 0;
    if ((p = strstr(ascii, SR_MessageId)))
      sprintf(id, ", %s %i", SR_MessageId, atoi(p +strlen(SR_MessageId) +1));

    if ((p = strstr(ascii, SR_Status)))
      // 3.1.14: include explanation
      //sprintf(status, ", %s %i", SR_Status, atoi(p +strlen(SR_Status) +1));
      snprintf(status, sizeof(status), ", %s %s", SR_Status, p +strlen(SR_Status) +1);

    writelogfile(LOG_NOTICE, 0, "SMS received (report%s%s), From: %s", id, status, sendr); 
  }

  *stored_concatenated = 0;
  if (do_internal_combine == 1)
  {
    int offset = 0; // points to the part count byte.
    int m_id;
    char storage_udh[SIZE_UDH_DATA] = {};
    int a_type;

    if ((a_type = get_concatenation(udh_data, &m_id, &p_count, &p_number)) > 0)
    {
      if (p_count > 1)
      {
        if (a_type == 1)
          sprintf(storage_udh, "05 00 03 ");
        else
          sprintf(storage_udh, "06 08 04 %02X ", (m_id & 0xFF00) >> 8);
        sprintf(strchr(storage_udh, 0), "%02X ", m_id & 0xFF);
        sprintf(strchr(storage_udh, 0), "%02X %02X ", p_count, p_number);        
        offset = (a_type == 1)? 12 : 15;
      }
    }

    if (offset)
    {
      // This is a part of a concatenated message.
      char con_filename[PATH_MAX];
      //int partcount;
      char st[1024];
      char messageid[6];
      int i;
      int found = 0;
      int udlen;
      int ftmp;
      char tmp_filename[PATH_MAX];
      int cmp_start;
      int cmp_length;
      char *p;
      int part;
      struct stat statbuf;

      // First we store it to the concatenated store of this device:
      // 3.1beta7: Own folder for storage and smsd's other saved files:
      sprintf(con_filename, CONCATENATED_DIR_FNAME, (*d_saved)? d_saved : d_incoming, DEVICE.name);
      if (!(fd = fopen(con_filename, "a")))
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot open file %s: %s", con_filename, strerror(errno)));
        alarm_handler0(LOG_ERR, tb);
        result = 0;
      }
      else
      {
        //UDH-DATA: 05 00 03 02 03 02 PDU....
        //UDH-DATA: 06 08 04 00 02 03 02 PDU....
        fprintf(fd, "%s%s\n", storage_udh/*udh_data*/, line2);
        fclose(fd);
        //partcount = octet2bin(udh_data +offset);
        userdatalength = 0;
        *ascii = '\0';
        sprintf(messageid, (offset == 12)? "%.2s" : "%.5s", storage_udh/*udh_data*/ + 9);
        i = octet2bin(line2);
        cmp_length = octet2bin(line2 +2 +i*2 +2);
        if (cmp_length%2 != 0)
          cmp_length++;
        // 3.1.5: fixed start position of compare:
        //cmp_start = 2 +i*2 +4;
        cmp_start = 2 +i*2 +6;

#ifdef DEBUGMSG
  printf("!! --------------------\n");
  printf("!! storage_udh=%s\n", storage_udh);
  printf("!! line2=%.50s...\n", line2);
  printf("!! messageid=%s\n", messageid);
  printf("!! cmp_start=%i\n", cmp_start);
  printf("!! cmp_length=%i\n", cmp_length);
  printf("!!\n");
#endif

        // Next we try to find each part, starting at the first one:
        fd = fopen(con_filename, "r");
        for (i = 1; i <= p_count/*partcount*/; i++)
        {
          found = 0;
          fseek(fd, 0, SEEK_SET);
          while (fgets(st, sizeof(st), fd))
          {
            p = st +(octet2bin(st) +1) *3;
            part = (strncmp(st +3, "00", 2) == 0)? octet2bin(st +15) : octet2bin(st +18);
            if (strncmp(st +9, messageid, strlen(messageid)) == 0 && part == i &&
                strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0)
            {
              found = 1;
              pdu_store_length += strlen(p) +5;
              break;
            }
          }

          // If some part was not found, we can take a break.
          if (!found)
            break;
        }

        if (!found)
        {
          fclose(fd);
          *stored_concatenated = 1;
        }
        else
        {
          incoming_pdu_store = (char *)malloc(pdu_store_length +1);
          if (incoming_pdu_store)
            *incoming_pdu_store = 0;

          for (i = 1; i <= p_count/*partcount*/; i++)
          {
            fseek(fd, 0, SEEK_SET);
            while (fgets(st, sizeof(st), fd))
            {
              p = st +(octet2bin(st) +1) *3;
              part = (strncmp(st +3, "00", 2) == 0)? octet2bin(st +15) : octet2bin(st +18);
              if (strncmp(st +9, messageid, strlen(messageid)) == 0 && part == i &&
                  strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0)
              {
                if (incoming_pdu_store)
                {
                  strcat(incoming_pdu_store, "PDU: ");
                  strcat(incoming_pdu_store, p);
                }
                // Correct part was found, concatenate it's text to the buffer:
                if (i == 1) // udh_data and _type are taken from the first part only.
                  udlen = splitpdu(p, DEVICE.mode, &alphabet, sendr, date, Time, ascii +userdatalength, smsc,
                                   &with_udh, udh_data, udh_type, &is_statusreport, &is_unsupported_pdu,
                                   from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);
                else
                  udlen = splitpdu(p, DEVICE.mode, &alphabet, sendr, date, Time, ascii +userdatalength, smsc,
                                   &with_udh, 0, 0, &is_statusreport, &is_unsupported_pdu,
                                   from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);

                if (alphabet==-1 && DEVICE.cs_convert==1)
                  udlen=gsm2iso(ascii +userdatalength,udlen,ascii +userdatalength,sizeof(ascii) -userdatalength);
                else if (alphabet == 2 && do_decode_unicode_text == 1)
                {
#ifndef USE_ICONV
                  udlen = decode_ucs2(ascii +userdatalength, udlen);
                  alphabet = 0;
#else
                  udlen = iconv_ucs2utf(ascii +userdatalength, udlen,
                                        sizeof(ascii) -userdatalength);
                  alphabet = 4;
#endif
                }
                userdatalength += udlen;
                break;
              }
            }
          }

          sprintf(tmp_filename,"%s/%s.XXXXXX",d_incoming,DEVICE.name);
          ftmp = mkstemp(tmp_filename);       
          fseek(fd, 0, SEEK_SET);
          while (fgets(st, sizeof(st), fd))
          {
            p = st +(octet2bin(st) +1) *3;
            if (!(strncmp(st +9, messageid, strlen(messageid)) == 0 &&
                strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0))
              //write(ftmp, &st, strlen(st));
              write(ftmp, st, strlen(st));
          }

          close(ftmp);
          fclose(fd);
          unlink(con_filename);
          rename(tmp_filename, con_filename);

          // 3.1beta7: If a file is empty now, remove it:
          if (stat(con_filename, &statbuf) == 0)
            if (statbuf.st_size == 0)
              unlink(con_filename);

          // UDH-DATA is not valid anymore:
          // *udh_data = '\0';
          // with_udh = 0;
          if (remove_concatenation(udh_data) > 0)
          {
            if (!(*udh_data))
            {
              with_udh = 0;
              *udh_type = 0;
            }
            else
            {
              if (explain_udh(udh_type, udh_data) < 0)
                if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
                  sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");
            }
          }
        } // if, all parts were found.
      }
    } // if (offset), received message had concatenation header with more than 1 parts. 
  } // do_internal_combine ends.

  if (p_count == 1)
  {
    if (!is_statusreport)
      writelogfile(LOG_NOTICE, 0, "SMS received, From: %s", sendr); 
  }
  else
    writelogfile(LOG_NOTICE, 0, "SMS received (part %i/%i), From: %s", p_number, p_count, sendr); 

  if (result)
  {
    if (*stored_concatenated)
      result = 0;
    else
    {
      // 3.1:
      //sprintf(filename, "%s/%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name);
      char timestamp[81];

      make_datetime_string(timestamp, sizeof(timestamp), 0, 0, date_filename_format);
      if (date_filename == 1)
        sprintf(filename, "%s/%s.%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, timestamp, DEVICE.name);
      else if (date_filename == 2)
        sprintf(filename, "%s/%s.%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name, timestamp);
      else
        sprintf(filename, "%s/%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name);

      close(mkstemp(filename));
      //Replace the temp file by a new file with same name. This resolves wrong file permissions.
      unlink(filename);
      if ((fd = fopen(filename, "w")))
      { 
        char *p;

        // 3.1beta7, 3.0.9: This header can be used to detect that a message has no usual content:
        if (is_unsupported_pdu)
          fprintf(fd, "Error: Cannot decode PDU, see text part for details.\n");
        if (*warning_headers)
          fprintf(fd, "%s", warning_headers);
        fprintf(fd, "%s %s\n", get_header_incoming(HDR_From, HDR_From2), sendr);
        if (*from_toa && *HDR_FromTOA2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_FromTOA, HDR_FromTOA2), from_toa);
        if (name[0] && *HDR_Name2 != '-')
          fprintf(fd,"%s %s\n", get_header_incoming(HDR_Name, HDR_Name2), name);
        if (smsc[0] && *HDR_FromSMSC2 != '-')
          fprintf(fd,"%s %s\n", get_header_incoming(HDR_FromSMSC, HDR_FromSMSC2), smsc);

        if (date[0] && Time[0] && *HDR_Sent2 != '-')
        {
          make_datetime_string(sent, sizeof(sent), date, Time, 0);
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Sent, HDR_Sent2), sent);
        }

        // Add local timestamp
        make_datetime_string(received, sizeof(received), 0, 0, 0);
        fprintf(fd, "%s %s\n", get_header_incoming(HDR_Received, HDR_Received2), received);

        if (*HDR_Subject2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Subject, HDR_Subject2), DEVICE.name);

        // 3.1.4:
        if (*HDR_Modem2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Modem, HDR_Modem2), DEVICE.name);
        if (DEVICE.number[0])
          if (*HDR_Number2 != '-')
            fprintf(fd, "%s %s\n", get_header_incoming(HDR_Number, HDR_Number2), DEVICE.number);

        if (!strstr(DEVICE.identity, "ERROR") && *HDR_Identity2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Identity, HDR_Identity2), DEVICE.identity);
        if (report >= 0 && *HDR_Report2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Report, HDR_Report2), (report)? yes_word : no_word);
        if (replace >= 0 && *HDR_Replace2 != '-')
          fprintf(fd, "%s %i\n", get_header_incoming(HDR_Replace, HDR_Replace2), replace);

        // 3.1: Flash message is now detected. Header printed only if flash was used.
        if (flash > 0 && *HDR_Flash2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Flash, HDR_Flash2), yes_word);

        if (incomplete > 0 && *HDR_Incomplete2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Incomplete, HDR_Incomplete2), yes_word);

        p = "";
        if (alphabet == -1)
        {
          if (DEVICE.cs_convert)
            p = "ISO";
          else
            p = "GSM";
        }
        else if (alphabet == 0)
          p = "ISO";
        else if (alphabet == 1)
          p = "binary";
        else if (alphabet == 2)
          p = "UCS2";
        else if (alphabet == 4)
          p = "UTF-8";
        else if (alphabet == 3)
          p = "reserved";

        // 3.1.5:
        if (incoming_utf8 == 1 && alphabet <= 0)
          p = "UTF-8";

        if (*HDR_Alphabet2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Alphabet, HDR_Alphabet2), p);

        if (udh_data[0])
          fprintf(fd,"%s %s\n", HDR_UDHDATA, udh_data);
        if (udh_type[0] && *HDR_UDHType2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_UDHType, HDR_UDHType2), udh_type);

        // 3.1beta7: new header:
        if (*HDR_Length2 != '-')
          fprintf(fd, "%s %i\n", get_header_incoming(HDR_Length, HDR_Length2), (alphabet == 2)? userdatalength /2 : userdatalength);

        // 3.1beta7: This header is only necessary with binary messages. With other message types
        // there is UDH-DATA header included if UDH is presented. "true" / "false" is now
        // presented as "yes" / "no" which may be translated to some other language.
        if (alphabet == 1)
          fprintf(fd, "%s %s\n", HDR_UDH, (with_udh)? yes_word : no_word);

        // 3.1beta7, 3.0.9: with value 2 unsupported pdu's were not stored.
        if (store_received_pdu == 3 ||
           (store_received_pdu == 2 && (alphabet == 1 || alphabet == 2)) ||
           (store_received_pdu >= 1 && is_unsupported_pdu == 1))
        {
          if (incoming_pdu_store)
            fprintf(fd,"%s", incoming_pdu_store);
          else
            fprintf(fd,"PDU: %s\n", line2);
        }

        // Show the error position (first) if possible:
        if (store_received_pdu >= 1 && is_unsupported_pdu == 1)
        {
          char *p;
          char *p2;
          int pos;
          int len = 1;
          int i = 0;

          if ((p = strstr(ascii, "Position ")))
          {
            if ((pos = atoi(p +9)) > 0)
            {
              if ((p = strchr(p +9, ',')))
                if ((p2 = strchr(p, ':')) && p2 > p +1)
                  len = atoi(p +1);
              fprintf(fd, "Pos: ");
              while (i++ < pos -1)
                if (i % 10)
                {   
                  if (i % 5)
                    fprintf(fd, ".");
                  else
                    fprintf(fd, "-");
                }
                else
                  fprintf(fd, "*");

              for (i = 0; i < len; i++)
                fprintf(fd, "^");
              fprintf(fd, "~here(%i)\n", pos);
            }
          }
        }
        // --------------------------------------------

        fprintf(fd,"\n");

        // UTF-8 conversion if necessary:
        if (incoming_utf8 == 1 && alphabet <= 0)
        {
          // 3.1beta7, 3.0.9: GSM alphabet is first converted to ISO
          if (alphabet == -1 && DEVICE.cs_convert == 0)
          {
            userdatalength = gsm2iso(ascii, userdatalength, ascii, sizeof(ascii));
            alphabet = 0; // filename_preview will need this information.
          }

          iso2utf8_file(fd, ascii, userdatalength);
        }
        else
          fwrite(ascii,1,userdatalength,fd);

        fclose(fd);

#ifdef DEBUGMSG
  if ((fd = fopen(filename, "r")))
  {
    char buffer[1024];

    printf("!! FILE: %s\n", filename);
    while (fgets(buffer, sizeof(buffer) -1, fd))
      printf("%s", buffer);
    fclose(fd);
  }
#endif

        if (filename_preview > 0)
        {
          char *text = NULL;
          char buffer[21];

          if (alphabet < 1)
          {
            if (alphabet == -1 && DEVICE.cs_convert == 0)
              userdatalength = gsm2iso(ascii, userdatalength, ascii, sizeof(ascii));
            text = ascii;
          }
          else if (alphabet == 1 || alphabet == 2)
          {
            strcpy(buffer, (alphabet == 1)? "binary" : "ucs2");
            text = buffer;
          }
          apply_filename_preview(filename, text, filename_preview);
        }

        writelogfile(LOG_INFO, 0, "Wrote an incoming %smessage file: %s", (incomplete)? "incomplete " : "", filename);

        result = is_statusreport; //return is_statusreport;
      }
      else
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot create file %s!", filename));
        alarm_handler0(LOG_ERR, tb);
        result = 0; //return 0;
      }
    }
  }

  if (incoming_pdu_store)
  {
    free(incoming_pdu_store);
    incoming_pdu_store = NULL;
  }
  return result;
}

/* =======================================================================
   Receive one SMS
   ======================================================================= */

int receivesms(int* quick, int only1st)  
// receive one SMS or as many as the modem holds in memory
// if quick=1 then no initstring
// if only1st=1 then checks only 1st memory space
// Returns 1 if successful
// Return 0 if no SM available
// Returns -1 on error
{
  int max_memory,used_memory;
  int start_time=time(0);  
  int found;
  int foundsomething=0;
  int statusreport;
  int sim = 0;
  char line1[1024];
  char line2[1024];
  char filename[PATH_MAX];
  char cmdline[PATH_MAX+PATH_MAX+32];
  int stored_concatenated;
  int memories;
  char *p;
  char *p2;
  char memory_list[1024];
  char delete_list[1024];
  size_t i;

  flush_smart_logging();

  if (terminate == 1)
    return 0;

  used_memory = 0;

#ifdef DEBUGMSG
  printf("receivesms(quick=%i, only1st=%i)\n",*quick,only1st);
#endif

  STATISTICS->status = 'r';
  writelogfile(LOG_INFO, 0, "Checking device for incoming SMS");

  if (*quick==0 && DEVICE.modem_disabled == 0)
  {
    if (initialize_modem_receiving())
    {
      STATISTICS->usage_r+=time(0)-start_time;

      flush_smart_logging();

      return -1;
    }
  }
  
  // Dual-memory handler:
  for (memories = 0; memories <= 2; memories++)
  {
    if (terminate == 1)
      break;

    *delete_list = 0;

    if (DEVICE.primary_memory[0] && DEVICE.secondary_memory[0] && DEVICE.modem_disabled == 0)
    {
      char command[128];
      char answer[1024];
      char *memory;

      memory = 0;
      if (memories == 1)
      {
        if (only1st)
          break;
        memory = DEVICE.secondary_memory;
      }
      else if (memories == 2)
        memory = DEVICE.primary_memory;

      if (memory)
      {
        sprintf(command, "AT+CPMS=\"%s\"\r", memory);

        // 3.1beta7: initially the value was giwen as ME or "ME" without multiple memories defined.
        // If there is ME,ME,ME or "ME","ME,"ME" given, all "'s are removed while reading the setup.
        // Now, if there is a comma in the final command string, "'s are added back:
        p = command;
        while (*p && (p = strchr(p, ',')))
        {
          if (strlen(command) > sizeof(command) -3)
            break;
          for (p2 = strchr(command, 0); p2 > p; p2--)
            *(p2 +2) = *p2;
          strncpy(p, "\",\"", 3);
          p += 3; 
        }

        writelogfile(LOG_INFO, 0, "Changing memory");
        put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);

        // 3.1.9:
        if (strstr(answer, "ERROR"))
        {
          writelogfile(LOG_ERR, 1, "The modem said ERROR while trying to change memory to %s", memory);
          return -1;
        }
      }

      if (memories == 2)
        break;
    }
    else if (memories > 0)
      break;

    *check_memory_buffer = 0;

    // Check how many memory spaces we really can read
    if (check_memory(&used_memory, &max_memory, memory_list, sizeof(memory_list), delete_list, sizeof(delete_list)) == 0)
      break;

    found=0;
    if (used_memory>0)
    {
      if (max_memory == 0 && memories == 1)
        max_memory = DEVICE.secondary_memory_max;

      // 3.1.5: memory list handling if method 2 or 3 is used:
      //for (sim=DEVICE.read_memory_start; sim<=DEVICE.read_memory_start+max_memory-1; sim++)
      if (!value_in(DEVICE.check_memory_method, 6, CM_CMGD, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
        sim = DEVICE.read_memory_start;

      for (;;)
      {
        if (!(*delete_list))
          if (terminate == 1)
            break;

        if (value_in(DEVICE.check_memory_method, 6, CM_CMGD, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
        {
          if (!(*memory_list))
            break;

          sim = atoi(memory_list);

          if ((p = strchr(memory_list, ',')))
            strcpyo(memory_list, p +1);
          else
            *memory_list = 0;
        }
        else
        {
          if (sim > DEVICE.read_memory_start +max_memory -1)
            break;
        }

        *line2 = 0;
        if (value_in(DEVICE.check_memory_method, 3, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
        {
          if (*check_memory_buffer)
          {
            // There can be more than one space before number, "+CMGL: %i" will not work.
            p = check_memory_buffer;
            while ((p = strstr(p, "+CMGL:")))
            {
              if (atoi(p +6) == sim)
                break;
              p += 6;
            }

            if (p)
            {
              if ((p = strchr(p, '\r')))
              {
                p++;
                if (*p == '\n')
                  p++;
                if ((p2 = strchr(p, '\r')))
                {
                  i = p2 -p;
                  if (i < sizeof(line2))
                  {
                    strncpy(line2, p, i);
                    line2[i] = 0;                
                    found = sim;
                  }
                }
              }
            }
          }

          if (!(*line2))
            writelogfile(LOG_ERR, 1, "CMGL PDU read failed with message %i, using CMGR.", sim);
        }

        if (!(*line2))
          found = readsim(sim, line1, line2);

        // 3.1.5: Should stop if there was timeout:
        if (found == -2)
        {
          *quick = 0;
          break;
        }

        if (found>=0)
        {
          foundsomething=1;
          *quick=1;
        
          //Create a temp file for received message
          //3.1beta3: Moved to the received2file function, filename is now a return value:
          //sprintf(filename,"%s/%s.XXXXXX",d_incoming,DEVICE.name);
          //close(mkstemp(filename));
          statusreport = received2file(line1, line2, filename, &stored_concatenated, 0);
          STATISTICS->received_counter++;

          if (stored_concatenated == 0)
          {
            if (eventhandler[0] || DEVICE.eventhandler[0])
            {
              char *handler = eventhandler;

              if (DEVICE.eventhandler[0])
                handler = DEVICE.eventhandler;

              snprintf(cmdline, sizeof(cmdline), "%s %s %s", handler, (statusreport)? "REPORT" : "RECEIVED", filename);
              exec_system(cmdline, EXEC_EVENTHANDLER);
            }
          }


          if (value_in(DEVICE.check_memory_method, 2, CM_CMGL_DEL_LAST, CM_CMGL_DEL_LAST_CHECK))
          {
            char tmp[32];

            sprintf(tmp, "%s%u", (*delete_list)? "," : "", found);
            if (strlen(delete_list) +strlen(tmp) < sizeof(delete_list))
              strcat(delete_list, tmp);            
          }
          else
          if (!value_in(DEVICE.check_memory_method, 1, CM_CMGL_SIMCOM))
            deletesms(found);

          used_memory--;
          if (used_memory<1) 
            break; // Stop reading memory if we got everything
        }
        if (only1st)
          break;

        //if (!value_in(DEVICE.check_memory_method, 6, CM_CMGD, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
          sim++;
      }
    }

    if (*delete_list)
      deletesms_list(delete_list);
  }

  STATISTICS->usage_r+=time(0)-start_time;
  if (foundsomething)
  {
    flush_smart_logging();

    return 1;
  }
  else
  {
    writelogfile(LOG_INFO, 0, (used_memory == 0)? "No SMS received" : "No SMS received (reading interrupted)");

    flush_smart_logging();

    return 0;
  }
}

/* ==========================================================================================
   Send a part of a message, this is physically one SM with max. 160 characters or 140 bytes
   ========================================================================================== */

int send_part(char* from, char* to, char* text, int textlen, int alphabet, int with_udh, char* udh_data,
              int quick, int flash, char* messageids, char* smsc, int report, int validity, int part, int parts, int replace_msg,
              int system_msg, int to_type, char *error_text)
// alphabet can be -1=GSM 0=ISO 1=binary 2=UCS2
// with_udh can be 0=off or 1=on or -1=auto (auto = 1 for binary messages and text message with udh_data)
// udh_data is the User Data Header, only used when alphabet= -1 or 0.
// With alphabet=1 or 2, the User Data Header should be included in the text argument.
// smsc is optional. Can be used to override config file setting.
// Output: messageids
// 3.1beta7: return value changed:
// 0 = OK.
// 1 = Modem initialization failed.
// 2 = Cancelled because of too many retries.
// 3 = Cancelled because shutdown request while retrying.
// error_text: latest modem response. Might have a value even if return value is ok (when retry helped).
{
  char pdu[1024];
  int retries;
  char command[128];
  char command2[1024];
  char answer[1024];
  char* posi1;
  char* posi2;
  char partstr[41];
  char replacestr[41];
  time_t start_time;
  char tmpid[10] = {0};

#ifdef DEBUGMSG
  printf("!! send_part(from=%s, to=%s, text=..., textlen=%i, alphabet=%i, with_udh=%i, udh_data=%s, quick=%i, flash=%i, messageids=..., smsc=%s, report=%i, validity=%i, part=%i, parts=%i, replace_msg=%i, system_msg=%i, to_type=%i)\n",
         from, to, textlen, alphabet, with_udh, udh_data, quick, flash, smsc, report, validity, part, parts, replace_msg, system_msg, to_type);
#endif
  if (error_text)
    *error_text = 0;
  start_time = time(0);
  // Mark modem as sending
  STATISTICS->status = 's';

  *partstr = 0;
  if (parts > 1)
    sprintf(partstr, " (part %i/%i)", part +1, parts);
  writelogfile(LOG_INFO, 0, "Sending SMS%s from %s to %s", partstr, from,to);

  // 3.1beta7: Now logged only if a message file contained Report:yes.
  if (report == 1 && !DEVICE.incoming)
    writelogfile(LOG_NOTICE, 0, "Cannot receive status report because receiving is disabled");

  if ((quick==0 || (*smsc && !DEVICE.smsc_pdu)) && DEVICE.sending_disabled == 0)
  {
    int i;

    i = initialize_modem_sending(smsc);
    if (i)
    {
      STATISTICS->usage_s+=time(0)-start_time;

      flush_smart_logging();

      return (i == 7)? 3 : 1;
    }
  }
  else
  {
    // 3.1:
    if (DEVICE.sending_disabled == 0 && DEVICE.check_network)
    {
      switch (wait_network_registration(1, 100))
      {
        case -1:
          STATISTICS->usage_s+=time(0)-start_time;
          flush_smart_logging();
          return 1;

        case -2:
          STATISTICS->usage_s+=time(0)-start_time;
          flush_smart_logging();
          return 3;
      }
    }
  }

  // Compose the modem command
  make_pdu(to,text,textlen,alphabet,flash,report,with_udh,udh_data,DEVICE.mode,pdu,validity, replace_msg, system_msg, to_type, smsc);
  if (strcasecmp(DEVICE.mode,"old")==0)
    sprintf(command,"AT+CMGS=%i\r",(int)strlen(pdu)/2);
  else
    sprintf(command,"AT%s+CMGS=%i\r", (DEVICE.verify_pdu)? "E1" : "", (int)strlen(pdu)/2-1); // 3.1.4: verify_pdu

  sprintf(command2,"%s\x1A",pdu);
  
  if (store_sent_pdu)
  {
    char *title = "PDU: ";

    if (!outgoing_pdu_store)
    {
      if ((outgoing_pdu_store = (char *)malloc(strlen(title) +strlen(pdu) +2)))
        *outgoing_pdu_store = 0;
    }
    else
      outgoing_pdu_store = (char *)realloc((void *)outgoing_pdu_store, strlen(outgoing_pdu_store) +strlen(title) +strlen(pdu) +2);

    if (outgoing_pdu_store)
      sprintf(strchr(outgoing_pdu_store, 0), "%s%s\n", title, pdu);
  }

  // 3.1.5: DEBUGGING:
  //if (DEVICE.sending_disabled == 1)
  if (DEVICE.sending_disabled == 1 ||
      (enable_smsd_debug && parts > 1 && part == 0 && strstr(smsd_debug, "drop1")) ||
      (enable_smsd_debug && parts > 2 && part == 1 && strstr(smsd_debug, "drop2"))
     )
  {
    writelogfile(LOG_NOTICE, 0, "Test run, NO actual sending:%s from %s to %s", partstr, from, to);
    writelogfile(LOG_DEBUG, 0, "PDU to %s: %s %s", to, command, pdu);

    // 3.1.12: Simulate sending time
    //sleep(1);
    t_sleep(4 + getrand(10));

    strcpy(messageids, "1");

    update_message_counter(1, DEVICE.name);

    STATISTICS->usage_s+=time(0)-start_time;
    STATISTICS->succeeded_counter++;

    writelogfile(LOG_NOTICE, 0, "SMS sent, Message_id: %s, To: %s, sending time %i sec.", messageids, to, time(0) -start_time);

    flush_smart_logging();

    return 0;
  }
  else
  {
    retries=0;
    while (1)
    {
      // Send modem command
      // 3.1.5: initial timeout changed 1 --> 2 and for PDU 6 --> 12.
      put_command(command, answer, sizeof(answer), 2, "(>)|(ERROR)");
      // Send message if command was successful
      if (!strstr(answer,"ERROR"))
      {
        put_command(command2, answer ,sizeof(answer), 12, EXPECT_OK_ERROR);

        // 3.1.14:
        if (DEVICE.verify_pdu)
        {
          char *buffer;

          if ((buffer = strdup(command2)))
          {
            int i;

            i = strlen(buffer);
            if (i > 1)
            {
              buffer[i - 1] = 0;

              if (strstr(answer, buffer))
                writelogfile(LOG_NOTICE, 0, "Verify PDU: OK");
              else
              {
                int src = 0;
                char *p = answer;

                while (buffer[src])
                {
                  if (*p && (p = strchr(p, buffer[src])))
                  {
                    p++;
                    src++;
                  }
                  else
                  {
                    writelogfile(LOG_ERR, 1, "Verify PDU: ERROR (pos: %s)", src);
                    writelogfile(LOG_ERR, 1, "Verify PDU: -> %s", buffer);
                    writelogfile(LOG_ERR, 1, "Verify PDU: <- %s", answer);
                    break;
                  }
                }

                if (buffer[src] == 0)
                  writelogfile(LOG_NOTICE, 0, "Verify PDU: OK (not exact)");
              }
            }

            free(buffer);
          }
        }
      }

      // 3.1.14:
      if (DEVICE.verify_pdu)
      {
        char answer2[1024];

        put_command("ATE0\r", answer2 ,sizeof(answer2), 1, EXPECT_OK_ERROR);
      }

      // Check answer
      if (strstr(answer,"OK"))
      {
        // If the modem answered with an ID number then copy it into the messageid variable.
        posi1=strstr(answer,"CMGS: ");
        if (posi1)
        {
          posi1+=6;
          posi2=strchr(posi1,'\r');
          if (! posi2) 
            posi2=strchr(posi1,'\n');
          if (posi2)
            posi2[0]=0;

          // 3.1:
          //strcpy(messageid,posi1);
          strcpy(tmpid, posi1);
          while (*tmpid == ' ')
            strcpyo(tmpid, tmpid +1);

          // 3.1.1:
          switch (DEVICE.messageids)
          {
            case 1:
              if (!(*messageids))
                strcpy(messageids, tmpid);
              break;

            case 2:
              strcpy(messageids, tmpid);
              break;

            case 3:
              if (*messageids)
                sprintf(strchr(messageids, 0), " %s", tmpid);
              else
                strcpy(messageids, tmpid);
              break;
          }

#ifdef DEBUGMSG
  printf("!! messageid=%s\n", tmpid);
#endif
        }

        *replacestr = 0;
        if (replace_msg)
          sprintf(replacestr, ", Replace_msg: %i", replace_msg);

        // 3.1:
        //writelogfile(LOG_NOTICE, 0, "SMS sent%s, Message_id: %s%s, To: %s, sending time %i sec.", partstr, tmpid, replacestr, to, time(0) -start_time);
        // 3.1.14: show retries
        *answer = 0;
        if (retries > 0)
          snprintf(answer, sizeof(answer), " Retries: %i.", retries);
        writelogfile(LOG_NOTICE, 0, "SMS sent%s, Message_id: %s%s, To: %s, sending time %i sec.%s", partstr, tmpid, replacestr, to, time(0) -start_time, answer);

        // 3.1.1:
        update_message_counter(1, DEVICE.name);

        STATISTICS->usage_s+=time(0)-start_time;
        STATISTICS->succeeded_counter++;

        flush_smart_logging();

        return 0;
      }  
      else 
      {
        // 3.1.14: show retries and trying time when stopping:
        int result = 0;

        // Set the error text:
        if (error_text)
        {
          strcpy(error_text, answer);
          cut_ctrl(error_text);
          cutspaces(error_text);
        }

        // 3.1.5:
        //writelogfile0(LOG_ERR, tb_sprintf("The modem said ERROR or did not answer."));
        if (!(*answer))
          writelogfile0(LOG_ERR, 1, tb_sprintf("The modem did not answer (expected OK)."));
        else
          writelogfile0(LOG_ERR, 1, tb_sprintf("The modem answer was not OK: %s", cut_ctrl(answer)));

        alarm_handler0(LOG_ERR, tb);

        if (++retries <= 2)
        {
          writelogfile(LOG_NOTICE, 1, "Waiting %i sec. before retrying", errorsleeptime);

          if (t_sleep(errorsleeptime))
            result = 3; // Cancel if terminating
          else if (initialize_modem_sending("")) // Initialize modem after error
            result = 1; // Cancel if initializing failed
        }
        else
          result = 2; // Cancel if too many retries

        if (result)
        {
          STATISTICS->usage_s += time(0) - start_time;
          STATISTICS->failed_counter++;
          writelogfile0(LOG_WARNING, 1, tb_sprintf("Sending SMS%s to %s failed, trying time %i sec. Retries: %i.", partstr, to, time(0) - start_time, retries - 1));
          alarm_handler0(LOG_WARNING, tb);
          flush_smart_logging();
          return result;
        }
      }
    }  
  }
}

/* =======================================================================
   Send a whole message, this can have many parts
   ======================================================================= */

int send1sms(int* quick, int* errorcounter)    
// Search the queues for SMS to send and send one of them.
// Returns 0 if queues are empty
// Returns -1 if sending failed (v 3.0.1 because of a modem)
// v 3.0.1 returns -2 if sending failed because of a message file, in this case
// there is no reason to block a modem.
// Returns 1 if successful
{
  char filename[PATH_MAX];
  char newfilename[PATH_MAX];
  char to[SIZE_TO];
  char from[SIZE_FROM];
  char smsc[SIZE_SMSC];
  char provider[SIZE_QUEUENAME];
  char text[MAXTEXT];
  int with_udh=-1;
  int had_udh = 0; // for binary message handling.
  char udh_data[SIZE_UDH_DATA];
  int textlen;
  char part_text[maxsms_pdu+1];
  int part_text_length;
  char directory[PATH_MAX];
  char cmdline[PATH_MAX+PATH_MAX+32];
  int q,queue;
  int part;
  int parts = 0;
  int maxpartlen;
  int eachpartlen;
  int alphabet;
  int success=0;
  int flash;
  int report;
  int split;
  int tocopy;
  int reserved;
  char messageids[SIZE_MESSAGEIDS] = {0};
  int found_a_file=0;
  int validity;
  int voicecall;
  int hex;
  int system_msg;
  int to_type;
  int terminate_written=0;
  int replace_msg = 0;
  char macros[SIZE_MACROS];
  int i;
  char *fail_text = 0;
  char error_text[2048];
  char voicecall_result[1024] = {0};
  char errortext[SIZE_TB] = {0};
  // 3.1.4:
  char *filename_preview_buffer = 0;

#ifdef DEBUGMSG
  printf("!! send1sms(quick=%i, errorcounter=%i)\n", *quick, *errorcounter);
#endif

  // Search for one single sms file  
  for (q = 0; q < NUMBER_OF_MODEMS; q++)
  {
    if (q == 1)
      if (DEVICE.queues[q][0] == 0)
        break;

    if (((queue=getqueue(DEVICE.queues[q],directory))!=-1) &&
       (getfile(DEVICE.trust_spool, directory, filename, 1)))
    {
      found_a_file=1;
      break;
    }
  }
 
  // If there is no file waiting to send, then do nothing
  if (found_a_file==0)
  { 
#ifdef DEBUGMSG
  printf("!! No file\n");
  printf("!! quick=%i errorcounter=%i\n",*quick,*errorcounter);
#endif

    flush_smart_logging();

    return 0;
  }

  readSMSheader(filename, 0, to,from,&alphabet,&with_udh,udh_data,provider,&flash,smsc,&report,&split,
                &validity, &voicecall, &hex, &replace_msg, macros, &system_msg, &to_type);

  // SMSC setting is allowed only if there is smsc set in the config file:
  if (DEVICE.smsc[0] == 0 && !DEVICE.smsc_pdu)
    smsc[0] = 0;

  // If the checkhandler has moved this message, some things are probably not checked:
  if (to[0]==0)
  {
    writelogfile0(LOG_NOTICE, 0, tb_sprintf("No destination in file %s",filename));
    alarm_handler0(LOG_NOTICE, tb);
    fail_text = "No destination";
    success = -2;
  }
#ifdef USE_ICONV
  else if (alphabet>3)
#else
  else if (alphabet>2)
#endif
  {
    writelogfile0(LOG_NOTICE, 0, tb_sprintf("Invalid alphabet in file %s",filename));
    alarm_handler0(LOG_NOTICE, tb);
    fail_text = "Invalid alphabet";
    success = -2;
  }
  else if (to_type == -2)
  {
    writelogfile0(LOG_NOTICE, 0, tb_sprintf("Invalid number type in file %s", filename));
    alarm_handler0(LOG_NOTICE, tb);
    fail_text = "Invalid number type";
    success = -2;
  }

  if (success == 0)
  {
    // Use config file setting if report is unset in file header
    if (report == -1)
      report = DEVICE.report;

    // 3.1beta7: With binary message udh data cannot be entered using the header:
// 3.1:
//    if (alphabet == 1)
//      *udh_data = 0;

    // Set a default for with_udh if it is not set in the message file.
    if (with_udh==-1)
    {
      if ((alphabet==1 || udh_data[0]) && !system_msg)
        with_udh=1;
      else
        with_udh=0;
    }

    // Save the udh bit, with binary concatenated messages we need to know if
    // there is user pdu in the begin of a message.
    had_udh = with_udh;

    // If the header includes udh-data then enforce udh flag even if it is not 1.
    if (udh_data[0])
      with_udh=1;

    // if Split is unset, use the default value from config file
    if (split==-1) 
      split=autosplit;

    // disable splitting if udh flag or udh_data is 1
    // 3.1beta7: binary message can have an udh:
    //if (with_udh && split)
// 3.1:
//    if (*udh_data && split)
    if (*udh_data && split && alphabet != 1)
    {
      split=0;
      // Keke: very old and possible wrong message, if there is no need to do splitting? Autosplit=0 prevents this message.
      writelogfile(LOG_INFO, 0, "Cannot split this message because it has an UDH.");      
    }
#ifdef DEBUGMSG
  printf("!! to=%s, from=%s, alphabet=%i, with_udh=%i, udh_data=%s, provider=%s, flash=%i, smsc=%s, report=%i, split=%i\n",to,from,alphabet,with_udh,udh_data,provider,flash,smsc,report,split);
#endif
    // If this is a text message, then read also the text    
    if (alphabet<1 || alphabet >= 2)
    {
#ifdef DEBUGMSG
  printf("!! This is %stext message\n", (alphabet >= 2)? "unicode " : "");
#endif
      maxpartlen = (alphabet >= 2)? maxsms_ucs2 : maxsms_pdu; // ucs2 = 140, pdu = 160
      readSMStext(filename, 0, DEVICE.cs_convert && (alphabet==0), text, &textlen, macros);
      // Is the message empty?
      if (textlen==0)
      {
        writelogfile0(LOG_NOTICE, 0, tb_sprintf("The file %s has no text",filename));
        alarm_handler0(LOG_NOTICE, tb);
        fail_text = "No text";
        parts=0;
        success = -2;
      }
      else
      {
        // 3.1.4:
        if (filename_preview > 0 && alphabet < 1 && !system_msg)
        {
          if ((filename_preview_buffer = (char *)malloc(textlen +1)))
          {
            memcpy(filename_preview_buffer, text, textlen);
            filename_preview_buffer[textlen] = 0;
            gsm2iso(filename_preview_buffer, textlen, filename_preview_buffer, textlen +1);
          }
        }
        // ------

#ifdef USE_ICONV
        if (alphabet == 3)
        {
          if (is_ascii_gsm(text, textlen))
            alphabet = -1;
          else
          {
            alphabet = 2;
            textlen = (int)iconv_utf2ucs(text, textlen, sizeof(text));
          }
        }
#endif
        // In how many parts do we need to split the text?
        if (split>0)
        {
          // 3.1beta7: Unicode part numbering now supported.
          //if (alphabet == 2) // With unicode messages
          //  if (split == 2)  // part numbering with text is not yet supported,
          //    split = 3;     // using udh numbering instead. 

          // if it fits into 1 SM, then we need 1 part
          if (textlen<=maxpartlen)
          {
            parts=1;
            reserved=0;
            eachpartlen=maxpartlen;
          }
          else if (split==2) // number with text
          {
            reserved = 4; // 1/9_
            if (alphabet == 2)
              reserved *= 2;
            eachpartlen = maxpartlen -reserved;
            parts = (textlen +eachpartlen -1) /eachpartlen;
            // If we have more than 9 parts, we need to reserve 6 chars for the numbers
            // And recalculate the number of parts.
            if (parts > 9)
            {
              reserved = 6; // 11/99_
              if (alphabet == 2)
                reserved *= 2;
              eachpartlen = maxpartlen -reserved;
              parts = (textlen +eachpartlen -1) /eachpartlen;  
              // 3.1beta7: there can be more than 99 parts:
              if (parts > 99)
              {
                reserved = 8; // 111/255_
                if (alphabet == 2)
                  reserved *= 2;
                eachpartlen = maxpartlen -reserved;
                parts = (textlen +eachpartlen -1) /eachpartlen;  

                // 3.1.1:
                if (parts > 255)
                {
                  writelogfile0(LOG_NOTICE, 0, tb_sprintf("The file %s has too long text",filename));
                  alarm_handler0(LOG_NOTICE, tb);
                  fail_text = "Too long text";
                  parts=0;
                  success = -2;
                }
              }
            }
          }
          else if (split==3) // number with udh
          {
            // reserve 7 chars for the UDH
            reserved=7;
            if (alphabet == 2) // Only six with unicode
              reserved = 6;
            eachpartlen = maxpartlen -reserved;
            parts = (textlen +eachpartlen -1) /eachpartlen;
            concatenated_id++;
            if (concatenated_id>255)
              concatenated_id=0;
          }
          else
          {
            // no numbering, each part can have the full size
            eachpartlen=maxpartlen;
            reserved=0;
            parts = (textlen +eachpartlen -1) /eachpartlen;
          }
        }
        else
        {
          // split is 0, too long message is just cutted.
          eachpartlen=maxpartlen;
          reserved=0;
          parts=1; 
        }

        if (parts>1)
          writelogfile(LOG_INFO, 0, "Splitting this message into %i parts of max %i characters%s.",
                       parts, (alphabet == 2)? eachpartlen /2 : eachpartlen, (alphabet == 2)? " (unicode)" : "");
      }
    }
    else
    {
#ifdef DEBUGMSG
  printf("!! This is a binary message.\n");
#endif      
      maxpartlen=maxsms_binary;
      if (hex == 1)
        readSMShex(filename, 0, text, &textlen, macros, errortext);
      else
        readSMStext(filename, 0, 0,text,&textlen, NULL);

      // 3.1:
      if (*udh_data)
      {
        int bytes = (strlen(udh_data) +1) / 3;
        int i;

        if (textlen <= (ssize_t)sizeof(text) -bytes)
        {
          memmove(text +bytes, text, textlen);
          for (i = 0; i < bytes; i++)
            text[i] = octet2bin(udh_data +i *3);
          textlen += bytes;
        }

        *udh_data = 0;
      }

      eachpartlen=maxpartlen;
      reserved=0;
      parts=1;
      // Is the message empty?
      if (textlen == 0)
      {
        writelogfile0(LOG_NOTICE, 0, tb_sprintf("The file %s has no data.",filename));
        alarm_handler0(LOG_NOTICE, tb);
        if (*errortext)
          fail_text = errortext;
        else
          fail_text = "No data";
        parts=0;
        success = -2;
      }

      // 3.1beta7: Is the message too long?:
      if (textlen > maxpartlen)
      {
        if (system_msg)
        {
          // System message can use single part only
          writelogfile0(LOG_NOTICE, 0,
            tb_sprintf("The file %s has too long data for system message: %i (max: %i).", filename, textlen, maxpartlen));
          alarm_handler0(LOG_NOTICE, tb);
          fail_text = "Too long data for system message";
          parts = 0;
          success = -2;
        }
        else
        if (!split)
        {
          // Binary messages are not sent partially.
          writelogfile0(LOG_NOTICE, 0,
            tb_sprintf("The file %s has too long data for single part (Autosplit: 0) sending: %i.", filename, textlen));
          alarm_handler0(LOG_NOTICE, tb);
          fail_text = "Too long data for single part sending";
          parts = 0;
          success = -2;
        }
        else
        {
          // Always use UDH numbering.
          split = 3;
          reserved = 6;
          eachpartlen = maxpartlen -reserved;
          parts = (textlen +eachpartlen -1) /eachpartlen;
          concatenated_id++;
          if (concatenated_id > 255)
            concatenated_id = 0;
        }
      }
    }
  } // success was ok after initial checks.
    
  // parts can now be 0 if there is some problems,
  // fail_text and success is also set.

  // Try to send each part
  if (parts > 0)
    writelogfile(LOG_INFO, 0, "I have to send %i short message for %s",parts,filename); 
#ifdef DEBUGMSG
  printf("!! textlen=%i\n",textlen);
#endif 
  // If sending concatenated message, replace_msg should not be used (otherwise previously
  // received part is replaced with a next one...
  if (parts > 1)
    replace_msg = 0;

  for (part=0; part<parts; part++)
  {
    if (split==2 && parts>1) // If more than 1 part and text numbering
    {
      sprintf(part_text, "%i/%i     ", part +1, parts);

      if (alphabet == 2)
      {
        for (i = reserved; i > 0; i--)
        {
          part_text[i *2 -1] = part_text[i -1];
          part_text[i *2 -2] = 0;
        }
      }

      tocopy = textlen -(part *eachpartlen);
      if (tocopy > eachpartlen)
        tocopy = eachpartlen;
#ifdef DEBUGMSG
  printf("!! tocopy=%i, part=%i, eachpartlen=%i, reserved=%i\n",tocopy,part,eachpartlen,reserved);
#endif 
      memcpy(part_text +reserved, text +(eachpartlen *part), tocopy);
      part_text_length = tocopy +reserved;
    }
    else if (split==3 && parts>1)  // If more than 1 part and UDH numbering
    {
      // in this case the numbers are not part of the text, but UDH instead
      tocopy = textlen -(part *eachpartlen);
      if (tocopy > eachpartlen)
        tocopy = eachpartlen;
#ifdef DEBUGMSG
  printf("!! tocopy=%i, part=%i, eachpartlen=%i, reserved=%i\n",tocopy,part,eachpartlen,reserved);
#endif 
      memcpy(part_text, text +(eachpartlen *part), tocopy);
      part_text_length = tocopy; 
      sprintf(udh_data,"05 00 03 %02X %02X %02X",concatenated_id,parts,part+1); 
      with_udh=1;  
    }
    else  // No part numbers
    {
      tocopy = textlen -(part *eachpartlen);
      if (tocopy > eachpartlen)
        tocopy = eachpartlen;
#ifdef DEBUGMSG
  printf("!! tocopy=%i, part=%i, eachpartlen=%i\n",tocopy,part,eachpartlen);
#endif 
      memcpy(part_text, text +(eachpartlen *part), tocopy);
      part_text_length = tocopy;
    }

    // Some modems cannot send if the memory is full. 
    if ((receive_before_send) && (DEVICE.incoming))
    {
      receivesms(quick, 1);

      // TODO: return value handling, if modem got broken...
    }

    // Voicecall ability:
    // 3.1beta7: added calling time.
    if (part == 0 && voicecall == 1)
    {
      char command[1024];
      char answer[1024];
      int i;
      int count = 3;
      char *p, *p2, *p3;
      int wait_delay = 0;
      time_t wait_time;
      char *expect = "(OK)|(NO CARRIER)|(BUSY)|(NO ANSWER)|(ERROR)|(DELAYED)";
      // 3.1.7:
      int saved_detect_message_routing = DEVICE.detect_message_routing;
      int saved_detect_unexpected_input = DEVICE.detect_unexpected_input;
      // 3.1.12:
      int call_lost = 0;

      DEVICE.detect_message_routing = 0;
      DEVICE.detect_unexpected_input = 0;

      if (DEVICE.modem_disabled == 1)
      {
        writelogfile(LOG_CRIT, 0, "Cannot make a voice call, MODEM IS DISABLED");
        fail_text = "Modem was disabled";
        success = -2;
      }
      else
      if (initialize_modem_sending(""))
      {
        writelogfile(LOG_CRIT, 1, "Cannot make a voice call, modem initialization failed");
        fail_text = "Modem initialization failed";
        success = -2;
      }
      else
      {
        // Automatic redialing should be turned off in the phone settings!

        part_text[part_text_length] = '\0';
        cutspaces(part_text);
        for (i = 0; part_text[i]; i++)
          part_text[i] = toupper((int)part_text[i]);

        // Currently the starting header is optional:
        if (strncmp(part_text, "TONE:", 5) == 0)
          strcpyo(part_text, part_text +5);

        if ((p = strstr(part_text, "TIME:")))
        {
          p2 = p +5;
          while (is_blank(*p2))
            p2++;
          p3 = p2;
          while (isdigitc(*p3))
            p3++;
          *p3 = 0;
          wait_delay = atoi(p2);
          strcpyo(p, p3 +1);
        }

        cutspaces(part_text);

        // If there is a space, the first part is taken as count:
        if ((p = strchr(part_text, ' ')))
        {
          *p = '\0';
          if ((count = atoi(part_text)) <= 0)
            count = 3;
          cutspaces(strcpyo(part_text, p +1));
        }
        if (!(*part_text))
          strcpy(part_text, "1,1,1,#,0,0,0,#,1,1,0,0,1");

        writelogfile(LOG_INFO, 0, "I have to make a voice call to %s, with (%i times) DTMF %s",
                     to,count,part_text);
        
        if (set_numberformat(NULL, to, to_type) == NF_INTERNATIONAL)
          sprintf(command, "ATD+%s;\r", to);
        else
          sprintf(command, "ATD%s;\r", to);

        if (DEVICE.voicecall_cpas || DEVICE.voicecall_clcc)
        {
          // OK is returned after ATD command. (BenQ M32)
          // Dest phone is off: after 25 sec "NO ANSWER"
          // Dest phone does not answer: after 1 min 40 sec "NO ANSWER"
          // Dest phone hooks: after couple of seconds "BUSY"

          // AT+CPAS return codes:
          // 0: ready (ME allows commands from TA/TE)
          // 1: unavailable (ME does not allow commands from TA/TE)
          // 2: unknown (ME is not guaranteed to respond to instructions)
          // 3: ringing (ME is ready for commands from TA/TE, but the ringer is active)
          // 4: call in progress (ME is ready for commands from TA/TE, but a call is in progress)
          // 5: asleep (ME is unable to process commands from TA/TE because it is in a low functionality state) Also all other values below 128 are reserved.

          // 3.1.12:
          int was_ringing = 0;

          wait_time = time(0);

          put_command(command, answer, sizeof(answer), 24, expect);

          for (;;)
          {
            change_crlf(cut_emptylines(cutspaces(strcpy(voicecall_result, answer))), ' ');
            usleep_until(time_usec() + 500000);

            if (strstr(answer, "NO CARRIER") ||
              strstr(answer, "BUSY") ||
              strstr(answer, "NO ANSWER") ||
              strstr(answer, "ERROR") ||
              strstr(answer, "DELAYED"))
            {
              *answer = 0;
              break;
            }

            if (DEVICE.voicecall_cpas && strstr(answer, "+CPAS:"))
            {
              if (strstr(answer, "4"))
                break;

              if (!was_ringing && strstr(answer, "3"))
                was_ringing = 1;

              if (was_ringing && strstr(answer, "0"))
              {
                strcpy(voicecall_result, "Hangup");
                writelogfile(LOG_INFO, 0, "The result of a voice call was %s", voicecall_result);
                *answer = 0;
                call_lost = 1;
                break;
              }
            }

            // 3.1.12:
            if (DEVICE.voicecall_clcc)
            {
              char tmp[64];
              int break_for = 0;
              int found_clcc = 0;

              p = strstr(answer, "+CLCC:");
              while (p)
              {
                // Check direction, 0 = Mobile Originated:
                getfield(p, 2, tmp, sizeof(tmp));
                if (!strcmp(tmp, "0"))
                {
                  // Check the number. Some modems do not show the + sign:
                  getfield(p, 6, tmp, sizeof(tmp));
                  if (!strcmp(to, (*tmp == '+')? tmp + 1 : tmp))
                  {
                    found_clcc = 1;

                    // State of the call (MO):
                    // 0 = Active.
                    // ( 1 = Held. )
                    // 2 = Dialing.
                    // 3 = Alerting.

                    getfield(p, 3, tmp, sizeof(tmp));
                    i = atoi(tmp);

                    if (i == 0)
                    {
                      strcpy(answer, "OK");
                      break_for = 1;
                      break;
                    }

                    if (!was_ringing && (i == 2 || i == 3))
                      was_ringing = 1;
                  }
                }

                p = strstr(p +1, "+CLCC:");
              }

              if (break_for)
                break;

              if (was_ringing && !found_clcc)
              {
                strcpy(voicecall_result, "Hangup");
                writelogfile(LOG_INFO, 0, "The result of a voice call was %s", voicecall_result);
                *answer = 0;
                call_lost = 1;
                break;
              }
            }

            if (time(0) > wait_time + 150)
            {
              strcpy(voicecall_result, "Timeout");
              writelogfile(LOG_INFO, 0, "The result of a voice call was %s", voicecall_result);
              *answer = 0;
              break;
            }

            put_command((DEVICE.voicecall_cpas)? "AT+CPAS\r" : "AT+CLCC\r", answer, sizeof(answer), 24, expect);
          }
        }
        else
        {
          if (!wait_delay)
          {
            // 3.1.7:
            // put_command(command, answer, sizeof(answer), 24, expect);
            i = put_command(command, answer, sizeof(answer), 24, expect);
            if (!(*answer) && i == -2)
              strcpy(answer, "Timeout");

            change_crlf(cut_emptylines(cutspaces(strcpy(voicecall_result, answer))), ' ');
            writelogfile(LOG_INFO, 0, "The result of a voice call was %s", voicecall_result);
          }
          else
          {
            put_command(command, 0, 0, 0, 0);
            writelogfile(LOG_DEBUG, 0, "Waiting for %i seconds", wait_delay);
            answer[0] = 0;
            wait_time = time(0);
            do
            {
              read_from_modem(answer, sizeof(answer), 2); // One read attempt is 200ms
              if (strstr(answer, "OK") ||
                  strstr(answer, "NO CARRIER") ||
                  strstr(answer, "BUSY") ||
                  strstr(answer, "NO ANSWER") ||
                  strstr(answer, "ERROR") ||
                  strstr(answer, "DELAYED"))
              {
                // 3.1.5beta9:
                cutspaces(answer);
                cut_emptylines(answer);
                if (log_single_lines)
                  change_crlf(answer, ' ');

                if (DEVICE.voicecall_ignore_modem_response)
                {
                  writelogfile(LOG_DEBUG, 0, "<- %s (ignored)", answer);
                  answer[0] = 0;
                }
                else
                {
                  writelogfile(LOG_DEBUG, 0, "<- %s", answer);
                  break;
                }
              }

              t_sleep(1);
            }
            while (time(0) < wait_time +wait_delay);
            change_crlf(cut_emptylines(cutspaces(strcpy(voicecall_result, answer))), ' ');
            // 3.1.7:
            writelogfile(LOG_INFO, 0, "The result of a voice call was %s", voicecall_result);
          }
        }

        // Some test results:
        // Dest phone is off: after 1 min 10 sec "NO ANSWER".
        // Dest phone does not answer: after 2 min 10 sec "", after CHUP "NO ANSWER".
        // Dest phone hooks: after couple of seconds "BUSY".
        // Dest phone answers: "OK".
        // CHUP after waiting 15 sec: "DELAYED".

        if (strstr(answer, "OK"))
        {
          // We are talking to the phone now.

          // ----------------------------------------------------------------------
          // 3.1.3: Security fix: used entered string was sent to the modem without
          // checking if it contains any/illegal AT commands.
          // Alternate VTS usage format is added too.
          //sprintf(command, "AT+VTS=%s\r", part_text);

          char *ptr = part_text;
          int cmd_length;
          char end_char;
          int tones = 0;
          char *quotation_mark;

          cmd_length = (DEVICE.voicecall_vts_list) ? 2 : 9;
          end_char = (DEVICE.voicecall_vts_list) ? ',' : ';';
          quotation_mark = (DEVICE.voicecall_vts_quotation_marks) ? "\"" : "";

          while (*ptr)
          {
            // Some modems support ABCD too but some not.
            if (!strchr("*#0123456789", *ptr))
              strcpyo(ptr, ptr + 1);
            else
              ptr++;
          }

          ptr = part_text;
          strcpy(command, "AT");
          if (DEVICE.voicecall_vts_list)
            strcat(command, "+VTS=");

          while (*ptr)
          {
            if (strlen(command) + cmd_length >= sizeof(command))
              break;
            if (DEVICE.voicecall_vts_list)
              sprintf(strchr(command, 0), "%c%c", *ptr, end_char);
            else
              sprintf(strchr(command, 0), "+VTS=%s%c%s%c", quotation_mark, *ptr, quotation_mark, end_char);
            ptr++;
            tones++;
          }

          if ((ptr = strrchr(command, end_char)))
            *ptr = '\r';

          // ----------------------------------------------------------------------

          for (i = 0; (i < count) && tones; i++)
          {
            t_sleep(3);
            put_command(command, answer, sizeof(answer), tones *0.39 +1, expect);
            if (strstr(answer, "ERROR"))
              if (i > 0)
                break;
          }
          t_sleep(1);
        }

        if (!call_lost)
        {
          if (DEVICE.voicecall_hangup_ath == 1 ||
              (DEVICE.voicecall_hangup_ath == -1 && voicecall_hangup_ath == 1))
            sprintf(command, "ATH\r");
          else
            sprintf(command, "AT+CHUP\r");

          put_command(command, answer, sizeof(answer), 1, expect);
          if (!(*voicecall_result))
            change_crlf(cut_emptylines(cutspaces(strcpy(voicecall_result, answer))), ' ');
        }

        success = 1;
      }

      DEVICE.detect_message_routing = saved_detect_message_routing;
      DEVICE.detect_unexpected_input = saved_detect_unexpected_input;

      break; // for parts...
    }
    else
    {
      // Try to send the sms      

      // If there is no user made udh (message header say so), the normal
      // concatenation header can be used. With user made udh the concatenation
      // information of a first part is inserted to the existing udh. Other but
      // first message part can be processed as usual.

      if (alphabet == 1 && part == 0 && parts > 1 && had_udh)
      {
        int n;

        *udh_data = 0;
        n = part_text[0];

        // Check if length byte has too high value:
        if (n >= part_text_length)
        {
          writelogfile0(LOG_NOTICE, 0, tb_sprintf("The file %s has incorrect first byte of UDH.",filename));
          alarm_handler0(LOG_NOTICE, tb);
          fail_text = "Incorrect first byte of UDH";
          success = -2;
          break; // for parts...
        }

        for (i = part_text_length -1; i > n; i--)
          part_text[i +5] = part_text[i];

        part_text[n +1] = 0;
        part_text[n +2] = 3;
        part_text[n +3] = concatenated_id;
        part_text[n +4] = parts;
        part_text[n +5] = part +1;
        part_text[0] = n +5;
        part_text_length += 5;
      }

      i = send_part((from[0])? from : DEVICE.number, to, part_text, part_text_length, alphabet, with_udh, udh_data,
                    *quick, flash, messageids, smsc, report, validity, part, parts, replace_msg, system_msg, to_type, error_text);

      if (i == 0)
      {
        // Sending was successful
        *quick=1;
        success=1;

        // Possible previous errors are ignored because now the modem worked well:
        *errorcounter=0;
      }
      else
      {
        // Sending failed
        *quick=0;
        success=-1;

        if (i == 1)
          fail_text = "Modem initialization failed";
        else if (*error_text)
          fail_text = error_text;
        else
          fail_text = "Unknown";

        // Do not send the next part if sending failed
        break;
      }
    }

    if (part<parts-1)
    {
      // If receiving has high priority, then receive all messages between sending each part.
      if (DEVICE.incoming==2)
      {
        receivesms(quick, 0);

        // TODO: return value handling, if modem got broken...
      }

      // Still part(s) left, possible termination is handled smoothly. This needs sms3 script.
      if (terminate == 1 && terminate_written == 0 && *infofile)
      {
        FILE *fp;
        char msg[256];

        sprintf(msg, "sending a multipart message, now part %i of %i.", part +1, parts);
        writelogfile(LOG_CRIT, 0, "Currently %s",msg);
        if ((fp = fopen(infofile, "a")))
        {
          fprintf(fp, "%s\n", msg);
          fclose(fp);
          terminate_written=1;
        }
        else
          writelogfile(LOG_CRIT, 0, "Infofile %s cannot be written.",infofile);
      }
    }
  }
    
  // Mark modem status as idle while eventhandler is running
  // Not if there has been trouble.
  if (!trouble_logging_started)
    STATISTICS->status = 'i';

  if (*messageids && DEVICE.messageids == 3)
    strcat(messageids, " .");

  if (success < 0)
  {
    // Sending failed

    // 3.1beta7:
    char remove_headers[4096];
    char add_headers[4096];
    // 3.1.1: Empty file is not moved to the failed folder.
    struct stat statbuf;
    int file_is_empty = 0;
    char timestamp[81];

    if (stat(filename, &statbuf) == 0)
      if (statbuf.st_size == 0)
        file_is_empty = 1;

    prepare_remove_headers(remove_headers, sizeof(remove_headers));
    *add_headers = 0;
    if (*HDR_Modem2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Modem, HDR_Modem2), DEVICE.name);

    // 3.1.4:
    if (DEVICE.number[0])
      if (*HDR_Number2 != '-')
        sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Number, HDR_Number2), DEVICE.number);

    if (!strstr(DEVICE.identity, "ERROR") && *HDR_Identity2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Identity, HDR_Identity2), DEVICE.identity);
    if (fail_text && *fail_text && *HDR_FailReason2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_FailReason, HDR_FailReason2), fail_text);

    // 3.1.5: Timestamp for failed message:
    make_datetime_string(timestamp, sizeof(timestamp), 0, 0, 0);
    if (*HDR_Failed2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n",
              get_header(HDR_Failed, HDR_Failed2), timestamp);

    change_headers(filename, remove_headers, outgoing_pdu_store, "%s", add_headers);

    // Move file into failed queue or delete
    if (d_failed[0] && !file_is_empty)
    {
      movefilewithdestlock_new(filename,d_failed, keep_filename, 0, process_title, newfilename);
      stop_if_file_exists("Cannot move",filename,"to",d_failed);

      // 3.1.1 Filename preview is applied for failed files too.
      if (filename_preview > 0 && !system_msg)
      {
        char *txt = NULL;
        char buffer[21];

        if (alphabet < 1)
        {
          if (filename_preview_buffer)
            txt = filename_preview_buffer;
          else
            txt = text;
        }
        else if (alphabet == 1 || alphabet == 2)
        {
          strcpy(buffer, (alphabet == 1)? "binary" : "ucs2");
          txt = buffer;
        }
        apply_filename_preview(newfilename, txt, filename_preview);
      }

      //writelogfile(LOG_INFO, process_title, "Moved file %s to %s",filename, (keep_filename)? d_failed : newfilename);
      writelogfile(LOG_INFO, 0, "Moved file %s to %s", filename, newfilename);
    }

    if (eventhandler[0] || DEVICE.eventhandler[0])
    {
      if (DEVICE.eventhandler[0])
        snprintf(cmdline, sizeof(cmdline), "%s FAILED %s %s", DEVICE.eventhandler, (d_failed[0] == 0 || file_is_empty)? filename : newfilename, messageids);
      else
        snprintf(cmdline, sizeof(cmdline), "%s FAILED %s %s", eventhandler, (d_failed[0] == 0 || file_is_empty)? filename : newfilename, messageids);
      exec_system(cmdline, EXEC_EVENTHANDLER);
    }

    if (d_failed[0] == 0 || file_is_empty)
    {
      unlink(filename);
      stop_if_file_exists("Cannot delete",filename,"","");
      writelogfile(LOG_INFO, 0, "Deleted file %s",filename);
    }
    unlockfile(filename);

    if (success == -1)
    {
      // Check how often this modem failed and block it if it seems to be broken
      (*errorcounter)++;
      if (*errorcounter>=blockafter)
      {
        writelogfile0(LOG_CRIT, 1, tb_sprintf("Fatal error: sending failed %i times. Blocking %i sec.", blockafter, blocktime));
        alarm_handler0(LOG_CRIT, tb);
        STATISTICS->multiple_failed_counter++;
        STATISTICS->status = 'b';
        t_sleep(blocktime);
        *errorcounter=0;
      }
    }
  }
  else
  {
    // Sending was successful
    char timestamp[81];
    char remove_headers[4096];
    char add_headers[8192];
    char *p;

    prepare_remove_headers(remove_headers, sizeof(remove_headers));
    *add_headers = 0;
    if (*HDR_Modem2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Modem, HDR_Modem2), DEVICE.name);

    // 3.1.4:
    if (DEVICE.number[0])
      if (*HDR_Number2 != '-')
        sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Number, HDR_Number2), DEVICE.number);

    make_datetime_string(timestamp, sizeof(timestamp), 0, 0, 0);
    if (*HDR_Sent2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n",
              get_header(HDR_Sent, HDR_Sent2), timestamp);
    if (report > 0 && messageids[0] != 0)
    {
      if (*HDR_MessageId2 != '-')
        sprintf(strchr(add_headers, 0), "%s %s\n",
                get_header(HDR_MessageId, HDR_MessageId2), messageids);  
    }
    if (!strstr(DEVICE.identity, "ERROR") && *HDR_Identity2 != '-')
      sprintf(strchr(add_headers, 0), "%s %s\n", get_header(HDR_Identity, HDR_Identity2), DEVICE.identity);
    if (*voicecall_result && *HDR_Result != '.')
      sprintf(strchr(add_headers, 0), "%s %s\n",
              get_header(HDR_Result, HDR_Result2), voicecall_result);  
    if (store_sent_pdu == 3 ||
       (store_sent_pdu == 2 && (alphabet == 1 || alphabet == 2)))
      p = outgoing_pdu_store;
    else
      p = NULL;
    change_headers(filename, remove_headers, p, "%s", add_headers);

    // Move file into sent queue or delete after eventhandler is executed.
    if (d_sent[0])
    {
      movefilewithdestlock_new(filename,d_sent, keep_filename, 0, process_title, newfilename);
      stop_if_file_exists("Cannot move",filename,"to",d_sent);

      // 3.1.1 Filename preview is applied for sent files too.
      if (filename_preview > 0 && !system_msg)
      {
        char *txt = NULL;
        char buffer[21];

        if (alphabet < 1)
        {
          if (filename_preview_buffer)
            txt = filename_preview_buffer;
          else
            txt = text;
        }
        else if (alphabet == 1 || alphabet == 2)
        {
          strcpy(buffer, (alphabet == 1)? "binary" : "ucs2");
          txt = buffer;
        }
        apply_filename_preview(newfilename, txt, filename_preview);
      }

      //writelogfile(LOG_INFO, process_title, "Moved file %s to %s",filename, (keep_filename)? d_sent : newfilename);
      writelogfile(LOG_INFO, 0, "Moved file %s to %s", filename, newfilename);
    }

    if (eventhandler[0] || DEVICE.eventhandler[0])
    {
      // Keke: Documentation says about the messsageid: 
      // " it is only used if you sent a message successfully with status report enabled."
      // Perhaps it should be removed when status report was not requested?
      if (DEVICE.eventhandler[0])
        snprintf(cmdline, sizeof(cmdline), "%s SENT %s %s", DEVICE.eventhandler, (d_sent[0] == 0)? filename : newfilename, messageids);
      else
        snprintf(cmdline, sizeof(cmdline), "%s SENT %s %s", eventhandler, (d_sent[0] == 0)? filename : newfilename, messageids);
      exec_system(cmdline, EXEC_EVENTHANDLER);
    }

    if (d_sent[0] == 0)
    {
      unlink(filename);
      stop_if_file_exists("Cannot delete",filename,"","");
      writelogfile(LOG_INFO, 0, "Deleted file %s",filename);
    }
    unlockfile(filename);
  }
    
#ifdef DEBUGMSG
  printf("quick=%i errorcounter=%i\n",*quick,*errorcounter);
  printf("returns %i\n",success);
#endif

  if (outgoing_pdu_store)
  {
    free(outgoing_pdu_store);
    outgoing_pdu_store = NULL;
  }

  // 3.1.4:
  if (filename_preview_buffer)
    free(filename_preview_buffer);

  flush_smart_logging();

  return success;
}

/* =======================================================================
   Administrative message sending. This is done without filesystem.
   ======================================================================= */

void send_admin_message(int *quick, int *errorcounter, char *text)
{
  char messageids[SIZE_MESSAGEIDS] = {0};
  char *to = NULL;
  time_t now;
  static time_t last_msgc_clear = 0;
  static int message_count = 0;
  char buffer[256];
  int textlen;

  (void) errorcounter;    // 3.1.7: remove warning.
  if (DEVICE.admin_to[0])
    to = DEVICE.admin_to;
  else if (admin_to[0])
    to = admin_to;

  if (to)
  {
    if (!last_msgc_clear)
      last_msgc_clear = time(0);

    if (DEVICE.adminmessage_count_clear > 0)
    {
      now = time(0);
      if (now >= last_msgc_clear + DEVICE.adminmessage_count_clear)
      {
        if (message_count > 0)
          writelogfile(LOG_INFO, 0, "Alert limit counter cleared, it was %i.", message_count);
        last_msgc_clear = now;
        message_count = 0;
      }
    }

    if (DEVICE.adminmessage_limit > 0)
    {
      if (message_count >= DEVICE.adminmessage_limit)
      {
        writelogfile(LOG_INFO, 0, "Alert limit reached, did not send: %s", text);
        return;
      }
    }

    if (!message_count)
      last_msgc_clear = time(0);
    message_count++;
    writelogfile(LOG_INFO, 0, "Sending an administrative message: %s", text);
    textlen = iso_utf8_2gsm(text, strlen(text), buffer, sizeof(buffer));

    send_part("Smsd3" /*from*/, 
                  to, buffer, textlen, 
                  0 /*ISO alphabet*/, 
                  0 /*with_udh*/, "" /*udh_data*/, 
                  *quick, 
                  0 /*flash*/,
                  messageids, 
                  "" /*smsc*/, 
                  DEVICE.report, 
                  -1 /*validity*/,
                  0, 1, 0, 0, -1 /*to_type*/,
                  0);
  }
}

/* =======================================================================
   Device-Spooler (one for each modem)
   ======================================================================= */

int cmd_to_modem(
	//
	//
	//
	char *command,
	int cmd_number
)
{
	int result = 1;
	char *cmd;
	char *p;
	char answer[500];
	char buffer[600];
	int fd;
	int log_retry = 3;
	int i;
	FILE *fp;
	int is_ussd = 0;	// 3.1.7

	if (!command || !(*command))
		return 1;

	if (!try_openmodem())
		return 0;

	if (cmd_number == 1 && DEVICE.needs_wakeup_at)
	{
		put_command("AT\r", 0, 0, 1, 0);
		usleep_until(time_usec() + 100000);
		read_from_modem(answer, sizeof(answer), 2);
	}

	if ((cmd = malloc(strlen(command) + 2)))
	{
		sprintf(cmd, "%s\r", command);
		// 3.1.5: Special case: AT+CUSD, wait USSD message:
		//put_command(*modem, device, cmd, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
		if (!strncasecmp(command, "AT+CUSD", 7) && strlen(command) > 9)
		{
			is_ussd++;
			put_command(cmd, answer, sizeof(answer), 3, "(\\+CUSD:)|(ERROR)");
		}
		else
		// 3.1.12:
		if (*cmd == '[' && strchr(cmd, ']'))
		{
			char *expect;

			if ((expect = strdup(cmd + 1)))
			{
				*(strchr(expect, ']')) = 0;
				put_command(strchr(cmd, ']') + 1, answer, sizeof(answer), 1, expect);
				free(expect);
			}
		}
		else
		// -------
			put_command(cmd, answer, sizeof(answer), 1, EXPECT_OK_ERROR);

		if (*answer)
		{
			char timestamp[81];

			make_datetime_string(timestamp, sizeof(timestamp), 0, 0, logtime_format);

			while ((p = strchr(answer, '\r')))
				*p = ' ';
			while ((p = strchr(answer, '\n')))
				*p = ' ';
			while ((p = strstr(answer, "  ")))
				strcpyo(p, p + 1);
			if (*answer == ' ')
				strcpyo(answer, answer + 1);
			p = answer + strlen(answer);
			while (p > answer && p[-1] == ' ')
				--p;
			*p = 0;

			if (is_ussd && DEVICE.ussd_convert == 1)
			{
#ifdef USE_ICONV
				// If answer of USSD received in unicode format, convert it to utf8
#define USSDOK "OK +CUSD: 2,\""
				i = 0;
				if (!strncasecmp(answer, USSDOK, sizeof(USSDOK) - 1) && sscanf(p = answer + sizeof(USSDOK) - 1, "%[0-9A-Fa-f]\",72%n", buffer, &i) == 1 && !p[i] && i > 4 && ((i -= 4) & 3) == 0)
#undef USSDOK
				{
					for (i = 0; buffer[i * 2]; i++)
					{
						unsigned v;

						sscanf(&buffer[i * 2], "%2X", &v);
						buffer[i] = (char) v;
					}
					i = (int) iconv_ucs2utf(buffer, i, sizeof(answer) - 2 - (p - answer)); //, 0);
					if (i != 0)
					{
						memcpy(p, buffer, i);
						p[i] = '"';
						p[i + 1] = 0;
					}
				}
#endif
			}
			else if (is_ussd && DEVICE.ussd_convert == 2)
			{
				if (strstr(answer, "+CUSD:"))
				{
					char *p1, *p2;

					if ((p1 = strchr(answer, '"')))
					{
						p1++;
						if ((p2 = strchr(p1, '"')))
						{
							snprintf(buffer, sizeof(buffer), "%.*s", (int)(p2 - p1), p1);
							if ((p = strdup(buffer)))
							{
								decode_7bit_packed(p, buffer, sizeof(buffer));
								free(p);
								cut_ctrl(buffer);
								if (strlen(answer) < sizeof(answer) - strlen(buffer) - 4)
									sprintf(strchr(answer, 0), " // %s", buffer);
							}
						}
					}
				}
			}
			// 3.1.11: HEX dump:
			else if (is_ussd && DEVICE.ussd_convert == 4)
			{
				if (strstr(answer, "+CUSD:"))
				{
					char *p1, *p2;

					if ((p1 = strchr(answer, '"')))
					{
						p1++;
						if ((p2 = strchr(p1, '"')))
						{
							snprintf(buffer, sizeof(buffer), "%.*s", (int)(p2 - p1), p1);
							if ((p = strdup(buffer)))
							{
								int c;
								int idx;

								*buffer = 0;
								for (idx = 0; strlen(p +idx) > 1; idx += 2)
								{
									sscanf(&p[idx], "%2X", &c);
									sprintf(strchr(buffer, 0), "%c", (char)c);
								}

								free(p);
								cut_ctrl(buffer);
								if (strlen(answer) < sizeof(answer) - strlen(buffer) - 4)
									sprintf(strchr(answer, 0), " // %s", buffer);
							}
						}
					}
				}
			}

			if (is_ussd && DEVICE.eventhandler_ussd[0])
			{
				char tmp_filename[PATH_MAX];
				char te_charset[41];
				size_t n, i;

				put_command("AT+CSCS?\r", te_charset, sizeof(te_charset), 1, EXPECT_OK_ERROR);
				cut_ctrl(cutspaces(te_charset));
				if (!(p = strchr(te_charset, '"')))
					*te_charset = 0;
				else
				{
					strcpyo(te_charset, p + 1);
					if ((p = strchr(te_charset, '"')))
						*p = 0;
				}
				if (!(*te_charset))
					strcpy(te_charset, "ERROR");

				sprintf(tmp_filename, "%s/smsd.XXXXXX", "/tmp");
				fd = mkstemp(tmp_filename);
				write(fd, answer, strlen(answer));
				write(fd, "\n", 1);
				close(fd);

				n = snprintf(buffer, sizeof(buffer), "%s USSD %s %s %s \"", DEVICE.eventhandler_ussd, tmp_filename, DEVICE.name, te_charset);

				for (i = 0; command[i] != '\0' && n < sizeof(buffer) - 2; n++, i++)
				{
					if (command[i] == '"')
						buffer[n++] = '\\';
					buffer[n] = command[i];
				}

				if (n < sizeof(buffer) - 2)
				{
					FILE *fp_tmp;

					buffer[n] = '"';
					buffer[n + 1] = '\0';

					exec_system(buffer, EXEC_EVENTHANDLER);

					if ((fp_tmp = fopen(tmp_filename, "r")))
					{
						if (fgets(buffer, sizeof(answer), fp_tmp))
							strcpy(answer, cut_ctrl(buffer));

						fclose(fp_tmp);
					}
				}
				else
					writelogfile(LOG_ERR, 0, "Buffer too small for execute USSD eventhadler for %s", DEVICE.name);

				unlink(tmp_filename);
			}

			if (DEVICE.dev_rr_logfile[0])
			{
				while (log_retry-- > 0)
				{
					// NOTE: log files use mode 640 as a fixed value.
					if ((fd = open(DEVICE.dev_rr_logfile, O_APPEND | O_WRONLY | O_CREAT, 0640)) >= 0)
					{
						snprintf(buffer, sizeof(buffer), "%s,%i, %s: CMD: %s: %s\n", timestamp, DEVICE.dev_rr_loglevel, DEVICE.name, command, answer);
						write(fd, buffer, strlen(buffer));
						close(fd);
						break;
					}

					if (log_retry > 0)
					{
						i = getrand(100);
						usleep_until(time_usec() + i * 10);
					}
					else
						writelogfile(LOG_ERR, 0, "Cannot open %s. %s", DEVICE.dev_rr_logfile, strerror(errno));
				}
			}
			else
				writelogfile(DEVICE.dev_rr_loglevel, 0, "CMD: %s: %s", command, answer);

			// 3.1.5: If signal quality was checked, explain it to the log:
			if (!strcasecmp(cmd, "AT+CSQ\r"))
				explain_csq(DEVICE.dev_rr_loglevel, 0, answer, DEVICE.signal_quality_ber_ignore);

			if (DEVICE.dev_rr_statfile[0])
			{
				// 3.1.6:
				//if ((fd = open(DEVICE.dev_rr_statfile, O_APPEND | O_WRONLY | O_CREAT, 0640)) >= 0)
				if ((fp = fopen(DEVICE.dev_rr_statfile, "a")))
				{
					//snprintf(buffer, sizeof(buffer), "%s,%i, %s: CMD: %s: %s\n", timestamp, LOG_NOTICE, DEVICE.name, command, answer);
					//write(fd, buffer, strlen(buffer));
					//close(fd);
					fprintf(fp, "%s,%i, %s: CMD: %s: %s\n", timestamp, LOG_NOTICE, DEVICE.name, command, answer);
					fclose(fp);
				}
				else
					writelogfile(LOG_ERR, 0, "Cannot open %s. %s", DEVICE.dev_rr_statfile, strerror(errno));
			}
		}

		free(cmd);
	}

	return result;
}

int run_rr()
{
  int result = 1;
  int modem_was_open;
  int i;
  FILE *fp;
  char st[4096];
  char *p;
  char cmdline[PATH_MAX + PATH_MAX + 32];
  int cmd_number = 0;

  modem_was_open = modem_handle >= 0;

  // 3.1.7: pre_run and post_run. dev_rr_statfile as $2.

  if (DEVICE.dev_rr[0])
  {
    p = "PRE_RUN";
    writelogfile(LOG_INFO, 0, "Running a regular_run (%s)", p);

    try_closemodem(1);

    // 3.1.9: added devicename.
    snprintf(cmdline, sizeof(cmdline), "%s %s \"%s\" %s", DEVICE.dev_rr, p, DEVICE.dev_rr_statfile, DEVICE.name);
    i = exec_system(cmdline, EXEC_RR_MODEM);
    if (i)
    {
      writelogfile0(LOG_ERR, 0, tb_sprintf("Regular_run %s %s returned %i", DEVICE.dev_rr, p, i));
      alarm_handler0(LOG_ERR, tb);
    }
  }

  if (DEVICE.dev_rr_statfile[0])
    unlink(DEVICE.dev_rr_statfile);

  // cmd_to_modem opens a modem if necessary.
  if (DEVICE.dev_rr_cmdfile[0])
  {
    if ((fp = fopen(DEVICE.dev_rr_cmdfile, "r")))
    {
      while (fgets(st, sizeof(st), fp))
      {
        // 3.1.12: remove only linebreaks:
        //cutspaces(st);
        //cut_ctrl(st);
        cut_crlf(st);

        if (*st && *st != '#')
        {
          if (!cmd_to_modem(st, ++cmd_number))
          {
            result = 0;
            break;
          }
        }
      }
      fclose(fp);
      unlink(DEVICE.dev_rr_cmdfile);
    }
  }

  if (result == 1)
  {
    p = DEVICE.dev_rr_cmd;
    while (*p)
    {
      if (!cmd_to_modem(p, ++cmd_number))
      {
        result = 0;
        break;
      }
      p = strchr(p, 0) + 1;
    }
  }

  if (DEVICE.dev_rr_post_run[0])
  {
    p = "POST_RUN";
    if (DEVICE.dev_rr[0] == 0 || strcmp(DEVICE.dev_rr, DEVICE.dev_rr_post_run))
      writelogfile(LOG_INFO, 0, "Running a regular_run_post_run %s", p);
    else
      writelogfile(LOG_INFO, 0, "Running a regular_run %s", p);

    try_closemodem(1);

    // 3.1.9: added devicename.
    snprintf(cmdline, sizeof(cmdline), "%s %s \"%s\" %s", DEVICE.dev_rr_post_run, p, DEVICE.dev_rr_statfile, DEVICE.name);
    i = exec_system(cmdline, EXEC_RR_POST_MODEM);
    if (i)
    {
      if (DEVICE.dev_rr[0] == 0 || strcmp(DEVICE.dev_rr, DEVICE.dev_rr_post_run))
        writelogfile0(LOG_ERR, 0, tb_sprintf("Regular_run_post_run %s %s returned %i", DEVICE.dev_rr_post_run, p, i));
      else
        writelogfile0(LOG_ERR, 0, tb_sprintf("Regular_run %s %s returned %i", DEVICE.dev_rr_post_run, p, i));
      alarm_handler0(LOG_ERR, tb);
    }
  }

  if (modem_was_open)
    try_openmodem();
  else
    try_closemodem(0);

  return result;
}

void do_ic_purge()
{
  int ic_purge;
  char con_filename[PATH_MAX];
  char tmp_filename[PATH_MAX +7];
  struct stat statbuf;
  FILE *fp;
  FILE *fptmp;
  char line[1024];
  int mcount = 0;
  int i;
  char *p;
  char buffer[LENGTH_PDU_DETAIL_REC +1];

  ic_purge = ic_purge_hours *60 +ic_purge_minutes;
  if (ic_purge <= 0)
    return;

  sprintf(con_filename, CONCATENATED_DIR_FNAME, (*d_saved)? d_saved : d_incoming, DEVICE.name);

  if (stat(con_filename, &statbuf) == 0)
    if (statbuf.st_size == 0)
      return;

  if ((fp = fopen(con_filename, "r")))
  {
    writelogfile0(LOG_INFO, 0, "Checking if concatenation storage has expired message parts");

    sprintf(tmp_filename,"%s.XXXXXX", con_filename);
    close(mkstemp(tmp_filename));
    unlink(tmp_filename);
    if (!(fptmp = fopen(tmp_filename, "w")))
    {
      writelogfile0(LOG_WARNING, 0, tb_sprintf("Concatenation storage handling aborted, creating %s failed", tmp_filename));
      alarm_handler0(LOG_WARNING, tb);
      fclose(fp);
      return;
    }

#ifdef DEBUGMSG
  printf("!! do_ic_purge, %i\n", ic_purge);
#endif

    while (fgets(line, sizeof(line), fp))
    {
      //UDH-DATA: 05 00 03 02 03 02 PDU....
      //UDH-DATA: 06 08 04 00 02 03 02 PDU....
#ifdef DEBUGMSG
  printf("!! %.50s...\n", line);
#endif
      i = octet2bin(line);
      if (i == 5 || i == 6)
      {
        p = (i == 5)? line +18 : line +21;
        if ((size_t)(p -line) < strlen(line))
        {
          *buffer = 0;
          i = get_pdu_details(buffer, sizeof(buffer), p, 0);
          if (i == 0)
          {
            time_t rawtime;
            struct tm *timeinfo;
            time_t now;
            time_t msgtime;
            char *p2;
            int pos_timestamp = 6; // 52;

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            now = mktime(timeinfo);

            p2 = buffer +pos_timestamp;
            timeinfo->tm_year = atoi(p2) +100;
            timeinfo->tm_mon = atoi(p2 +3) -1;
            timeinfo->tm_mday = atoi(p2 +6);
            timeinfo->tm_hour = atoi(p2 +9);
            timeinfo->tm_min = atoi(p2 +12);
            timeinfo->tm_sec = atoi(p2 +15);
            msgtime = mktime(timeinfo);

            if (ic_purge *60 > now - msgtime)
            {
#ifdef DEBUGMSG
  printf("!! %s", buffer);
  printf("!! remaining: %i\n", (int)(ic_purge - (now - msgtime) /60));
#endif
              fputs(line, fptmp);
            }
            else
            if (ic_purge_read)
            {
              char filename[PATH_MAX];

              // Remove line termination (if PDU is printed to the message file):
              while (strlen(p) > 1 && strchr("\r\n", p[strlen(p) -1]))
                p[strlen(p) -1] = 0;

              received2file("", p, filename, &i, 1);
              mcount++;
            }
          }
        }
      }
    }

    fclose(fp);
    fclose(fptmp);

    if (mcount)
    {
      unlink(con_filename);
      rename(tmp_filename, con_filename);
    }
    else
      unlink(tmp_filename);
  }
}

// 3.1.7:
int send_startstring()
{

  // 3.1.12:
  if (DEVICE.modem_disabled)
    return 0;

  if (DEVICE.startstring[0])
  {
    char answer[500];
    int retries = 0;
    char *p;

    writelogfile(LOG_INFO, 0, "Sending start string to the modem");

    do
    {
      retries++;
      put_command(DEVICE.startstring, answer, sizeof(answer), 2, EXPECT_OK_ERROR);
      if (strstr(answer, "ERROR"))
        if (retries < 2)
          t_sleep(1);
    }
    while (retries < 2 && !strstr(answer, "OK"));
    if (strstr(answer, "OK") == 0)
    {
      p = get_gsm_error(answer);
      writelogfile0(LOG_ERR, 1, tb_sprintf("Modem did not accept the start string%s%s", (*p) ? ", " : "", p));
      alarm_handler0(LOG_ERR, tb);
    }

    flush_smart_logging();

    if (DEVICE.startsleeptime > 0)
    {
      writelogfile(LOG_INFO, 0, "Spending sleep time after starting (%i sec)", DEVICE.startsleeptime);
      if (t_sleep(DEVICE.startsleeptime))
        return 1;
    }
  }

  return 0;
}

// 3.1.7:
int send_stopstring()
{

  // 3.1.12:
  if (DEVICE.modem_disabled)
    return 0;

  if (DEVICE.stopstring[0])
  {
    char answer[500];
    int retries = 0;
    char *p;

    if (!try_openmodem())
      writelogfile(LOG_ERR, 1, "Cannot send stop string to the modem");
    else
    {
      writelogfile(LOG_INFO, 0, "Sending stop string to the modem");

      do
      {
        retries++;
        put_command(DEVICE.stopstring, answer, sizeof(answer), 2, EXPECT_OK_ERROR);
        if (strstr(answer, "ERROR"))
          if (retries < 2)
            t_sleep(1);
      }
      while (retries < 2 && !strstr(answer, "OK"));
      if (strstr(answer, "OK") == 0)
      {
        p = get_gsm_error(answer);
        writelogfile0(LOG_ERR, 1, tb_sprintf("Modem did not accept the stop string%s%s", (*p) ? ", " : "", p));
        alarm_handler0(LOG_ERR, tb);
      }

      flush_smart_logging();
    }
  }

  return 0;
}

// 3.1.7: Check if a modem is suspended.
int check_suspend(int initialized)
{

  if (break_suspend)
    return 0;

  if (*suspend_filename)
  {
    int suspended = 0;
    FILE *fp;
    int i;
    char name[33];
    char line[PATH_MAX];
    int found;
    int modem_was_open;
    int dt;

    modem_was_open = modem_handle >= 0;
    snprintf(name, sizeof(name), "%s:", DEVICE.name);

    while ((fp = fopen(suspend_filename, "r")))
    {
      found = 0;
      while (fgets(line, sizeof(line), fp))
      {
        cutspaces(cut_ctrl(line));
        if (!strncmp(line, name, strlen(name)) || !strncmp(line, "ALL:", 4))
        {
          found = 1;

          if (!suspended)
          {
            if (initialized)
              send_stopstring();
            try_closemodem(1);
            strcpyo(line, strchr(line, ':') +1);
            cutspaces(line);
            writelogfile(LOG_NOTICE, 0, "Suspend started. %s", line);
            suspended = 1;
          }

          break;
        }
      }

      fclose(fp);

      if (suspended && (!found || break_suspend))
      {
        writelogfile(LOG_NOTICE, 0, (break_suspend)? "Suspend break." : "Suspend ended.");
        suspended = 0;

        if (modem_was_open)
          if (try_openmodem())
            if (initialized)
              if (send_startstring())
                return 1;
      }

      if (!suspended)
        break;

      dt = (delaytime > 10)? 10 : (delaytime < 1)? 1 : delaytime;
      for (i = 0; i < dt; i++)
      {
        if (terminate == 1)
        {
          // Do not send stop string when terminating:
          DEVICE.stopstring[0] = 0;

          return 1;
        }
        t_sleep(1);
      }

      continue;
    }
  }

  return 0;
}

void devicespooler()
{
  int workless;
  int quick=0;
  int errorcounter;
  int i;
  time_t now;
  time_t last_msgc_clear;
  time_t last_rr;
  time_t last_ic_purge;
  time_t started_sending;
  int continuous_sent; // 3.1.14.
  char *p = "";

  // Load initial modemname.counter value:
  update_message_counter(0, DEVICE.name);

  *smsd_debug = 0;

  i = LOG_NOTICE;
  if (DEVICE.outgoing && !DEVICE.incoming)
    p = " Will only send messages.";  
  else if (!DEVICE.outgoing && DEVICE.incoming)
    p = " Will only receive messages.";
  else if (!DEVICE.outgoing && !DEVICE.incoming)
  {
    p = " Nothing to do with a modem: sending and receiving are both disabled!";
    i = LOG_CRIT;
  }
  writelogfile(i, 0, "Modem handler %i has started. PID: %i.%s", process_id, (int)getpid(), p);

  // 3.1beta7: This message is printed to stderr while reading setup. Now also
  // log it and use the alarmhandler. Setting is cleared. Later this kind of
  // message is only given if there is Report:yes in the message file.
  if (DEVICE.report == 1 && !DEVICE.incoming && DEVICE.outgoing)
  {
    writelogfile0(LOG_WARNING, 0,
      tb_sprintf("Cannot receive status reports because receiving is disabled on modem %s", DEVICE.name));
    alarm_handler0(LOG_WARNING, tb);
    DEVICE.report = 0;
  }

  errorcounter=0;  
  concatenated_id = getrand(255);
  
  if (check_suspend(0))
    return;

  // Open serial port or return if not successful
  if (!try_openmodem())
    return;

  if (DEVICE.sending_disabled == 1 && DEVICE.modem_disabled == 0)
  {
    printf("%s: Modem handler %i is in testing mode, SENDING IS DISABLED\n", process_title, process_id);
    writelogfile(LOG_CRIT, 0, "Modem handler %i is in testing mode, SENDING IS DISABLED", process_id);
  }

  if (DEVICE.modem_disabled == 1)
  {
    printf("%s: Modem handler %i is in testing mode, MODEM IS DISABLED\n", process_title, process_id);
    writelogfile(LOG_CRIT, 0, "Modem handler %i is in testing mode, MODEM IS DISABLED", process_id);

    DEVICE.sending_disabled = 1;
  }

  if (DEVICE.priviledged_numbers[0])
  {
    char buffer[PATH_MAX];

    sprintf(buffer, "Using priviledged_numbers: ");
    p = DEVICE.priviledged_numbers;
    while (*p)
    {
      if (p != DEVICE.priviledged_numbers)
        strcat(buffer, ",");
      strcat(buffer, p);
      p = strchr(p, 0) +1;
    }
    writelogfile(LOG_NOTICE, 0, buffer);
  }

  // 3.1.7: Report check memory method only if modem will read incoming messages:
  if (DEVICE.incoming)
  {
    p = "Using check_memory_method ";
    i = DEVICE.check_memory_method;
    switch (i)
    {
      case CM_NO_CPMS:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_NO_CPMS);
        break;

      case CM_CPMS:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CPMS);
        break;

      case CM_CMGD:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGD);
        break;

      case CM_CMGL:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGL);
        break;

      case CM_CMGL_DEL_LAST:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGL_DEL_LAST);
        break;

      case CM_CMGL_CHECK:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGL_CHECK);
        break;

      case CM_CMGL_DEL_LAST_CHECK:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGL_DEL_LAST_CHECK);
        break;

      case CM_CMGL_SIMCOM:
        writelogfile(LOG_NOTICE, 0, "%s%i: %s", p, i, CM_S_CMGL_SIMCOM);
        break;
    }
  }

  if (DEVICE.read_timeout != 5)
      writelogfile(LOG_NOTICE, 0, "Using read_timeout %i seconds.", DEVICE.read_timeout);

  if (DEVICE.communication_delay > 0)
      writelogfile(LOG_NOTICE, 0, "Using communication_delay between new commands %i milliseconds.", DEVICE.communication_delay);

#ifdef DEBUGMSG
  printf("!! Entering endless send/receive loop\n");
#endif    

  last_msgc_clear = time(0);
  last_rr = 0;
  last_ic_purge = 0;

  // 3.1.12: Allocate memory for check_memory_buffer:
  if (DEVICE.incoming)
  {
    check_memory_buffer_size = select_check_memory_buffer_size();
    if (!(check_memory_buffer = (char *)malloc(check_memory_buffer_size)))
    {
      writelogfile0(LOG_CRIT, 1, tb_sprintf("Did not get memory for check_memory_buffer (%i). Stopping.", check_memory_buffer_size));
      alarm_handler0(LOG_CRIT, tb);
      return;
    }
  }

  if (send_startstring())
    return;

  // 3.1.1: If a modem is used for sending only, it's first initialized.
  // 3.1.12: Check if a modem is disabled.
  if (DEVICE.outgoing && !DEVICE.incoming && !DEVICE.modem_disabled)
  {
    if (initialize_modem_sending(""))
    {
      writelogfile0(LOG_CRIT, 1, tb_sprintf("Failed to initialize modem %s. Stopping.", DEVICE.name));
      alarm_handler0(LOG_CRIT, tb);
      return;
    }
    else
      writelogfile(LOG_NOTICE, 0, "Waiting for messages to send...");
  }

  // Copy device value to global value:
  if (DEVICE.max_continuous_sending != -1)
    max_continuous_sending = DEVICE.max_continuous_sending;

  if (max_continuous_sending < 0)
    max_continuous_sending = 0;

  flush_smart_logging();

  while (terminate == 0) /* endless loop */
  {
    workless=1;
    break_workless_delay = 0;
    workless_delay = 0;

    started_sending = time(0);
    continuous_sent = 0;

    while (!terminate && DEVICE.outgoing)
    {
      if (check_suspend(1))
        return;

      if (DEVICE.message_count_clear > 0)
      {
        now = time(0);
        if (now >= last_msgc_clear + DEVICE.message_count_clear)
        {
          if (message_count > 0)
            writelogfile(LOG_NOTICE, 0, "Message limit counter cleared, it was %i.", message_count);
          last_msgc_clear = now;
          message_count = 0;
        }
      }

      if (DEVICE.message_limit > 0)
        if (message_count >= DEVICE.message_limit)
          break;

      if (!strncmp(shared_buffer, DEVICE.name, strlen(DEVICE.name)))
      {
        char msg[SIZE_SHARED_BUFFER];

        strcpy(msg, shared_buffer);
        *shared_buffer = 0;

        if ((p = strchr(msg, ' ')))
        {
          writelogfile(LOG_NOTICE, 0, "Mainprocess asked to send: %s", p +1);
          send_admin_message(&quick, &errorcounter, p +1);
        }
      }

      if (!try_openmodem())
        return;

      i = send1sms(&quick, &errorcounter);

      if (i > 0)
      {
        if (!message_count)
          last_msgc_clear = time(0);

        message_count++;
        continuous_sent++;

        if (DEVICE.message_limit > 0 &&
            message_count == DEVICE.message_limit)
        {
          char msg[MAXTEXT];

          writelogfile0(LOG_WARNING, 0, tb_sprintf("Message limit %i is reached.", DEVICE.message_limit));
          alarm_handler0(LOG_WARNING, tb);

          sprintf(msg, "Smsd3: %s: Message limit %i is reached.", process_title, DEVICE.message_limit);
          send_admin_message(&quick, &errorcounter, msg);
        }

        if (max_continuous_sending > 0)
        {
          if (time(0) >= started_sending +max_continuous_sending)
          {
            writelogfile0(LOG_DEBUG, 0, "Max continuous sending time reached, will do other tasks and then continue.");
            workless = 0;

            if (continuous_sent)
            {
              time_t seconds;

              seconds = time(0) - started_sending;
              writelogfile(LOG_INFO, 0, "Sent %d messages in %d sec. Average time for one message: %.1f sec.", continuous_sent, seconds, (double)seconds / continuous_sent);
            }

            break;
          }
        }
      }
      else
        if (i != -2) // If there was a failed messsage, do not break.
          break;

      workless=0;

      if (DEVICE.incoming == 2) // repeat only if receiving has low priority
        break;

      if (terminate == 1)
        return;

      flush_smart_logging();
    }

    if (terminate == 1)
      return;

    // Receive SM
    if (DEVICE.incoming)
    {
      if (check_suspend(1))
        return;

      if (!try_openmodem())
        return;

      // In case of (fatal or permanent) error return value is < 0:
      if (receivesms(&quick, 0) > 0) 
        workless = 0;

      flush_smart_logging();

      if (routed_pdu_store)
      {
        char *term;
        char filename[PATH_MAX];
        int stored_concatenated;
        int statusreport;
        char cmdline[PATH_MAX+PATH_MAX+32];

        writelogfile0(LOG_INFO, 0, "Handling saved routed messages / status reports");

        p = routed_pdu_store;
        while (*p)
        {
          if (!(term = strchr(p, '\n')))
            break;

          *term = 0;

          statusreport = received2file("", p, filename, &stored_concatenated, 0);
          STATISTICS->received_counter++;
          if (stored_concatenated == 0)
          {
            if (eventhandler[0] || DEVICE.eventhandler[0])
            {
              char *handler = eventhandler;

              if (DEVICE.eventhandler[0])
                handler = DEVICE.eventhandler;

              snprintf(cmdline, sizeof(cmdline), "%s %s %s", handler, (statusreport)? "REPORT" : "RECEIVED", filename);
              exec_system(cmdline, EXEC_EVENTHANDLER);
            }
          }

          p = term +1;
        }

        free(routed_pdu_store);
        routed_pdu_store = NULL;
      }

      if (terminate == 1)
        return;
    }

    if (DEVICE.phonecalls == 1)
      readphonecalls();

    if (DEVICE.dev_rr_interval > 0 && DEVICE.modem_disabled == 0)
    {
      now = time(0);
      if (now >= last_rr +DEVICE.dev_rr_interval)
      {
        last_rr = now;
        if (!run_rr())
          return;
      }
    }

    if (DEVICE.internal_combine == 1 ||
        (DEVICE.internal_combine == -1 && internal_combine == 1))
    {
      if ((ic_purge_hours *60 +ic_purge_minutes) > 0)
      {
        now = time(0);
        if (now >= last_ic_purge +ic_purge_interval)
        {
          last_ic_purge = now;
          do_ic_purge();
        }
      }
    }

    if (DEVICE.incoming && keep_messages)
    {
      writelogfile0(LOG_WARNING, 0, tb_sprintf("Messages are kept, stopping."));
      try_closemodem(0);
      kill((int)getppid(), SIGTERM);
      return;
    }

    break_suspend = 0;
    if (check_suspend(1))
      return;

    if (workless==1) // wait a little bit if there was no SM to send or receive to save CPU usage
    {
      try_closemodem(0);

      // Disable quick mode if modem was workless
      quick=0;

      if (!trouble_logging_started)
        STATISTICS->status = 'i';

      workless_delay = 1;
      for (i=0; i<delaytime; i++)
      {
        if (terminate == 1)
          return;
        if (break_workless_delay)
          break;

        if (DEVICE.dev_rr_interval > 0 && !DEVICE.modem_disabled)
        {
          now = time(0);
          if (now >= last_rr +DEVICE.dev_rr_interval)
          {
            last_rr = now;
            if (!run_rr())
              return;
          }
        }

        t_sleep(1);
      }
      workless_delay = 0;
    }

    flush_smart_logging();
  }

}

/* =======================================================================
   Termination handler
   ======================================================================= */

// Stores termination request when termination signal has been received 
   
void soft_termination_handler (int signum)
{

  (void) signum;          // 3.1.7: remove warning.
  if (process_id==-1)
  {
    signal(SIGTERM,SIG_IGN);
    signal(SIGINT,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    signal(SIGUSR1,SIG_IGN);

    // 3.1.2: Signal handlers are now silent.
#ifdef DEBUG_SIGNALS_NOT_FOR_PRODUCTION
    writelogfile(LOG_CRIT, 0, "Smsd mainprocess received termination signal. PID: %i.", (int)getpid());
    if (signum==SIGINT)
      printf("Received SIGINT, smsd will terminate now.\n");
#endif

    sendsignal2devices(SIGTERM);

#ifdef DEBUG_SIGNALS_NOT_FOR_PRODUCTION
    if (*run_info)
    {
      printf("%s: Currently running: %s. Will wait until it is completed.\n", process_title, run_info);
      writelogfile(LOG_CRIT, 0, "Currently running: %s. Will wait until it is completed.", run_info);
    }
#endif

  }
  else if (process_id>=0)
  {
    signal(SIGTERM,SIG_IGN);
    signal(SIGINT,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    signal(SIGUSR1,SIG_IGN);
    // process_id has always the same value like device when it is larger than -1

#ifdef DEBUG_SIGNALS_NOT_FOR_PRODUCTION
    writelogfile(LOG_CRIT, 0, "Modem handler %i has received termination signal, will terminate after current task has been finished. PID: %i.", process_id, (int)getpid());
    if (*run_info)
    {
      printf("%s: Currently running: %s. Will wait until it is completed.\n", process_title, run_info);
      writelogfile(LOG_CRIT, 0, "Currently running: %s. Will wait until it is completed.", run_info);
    }
#endif

  }
  terminate=1;
}

void abnormal_termination(int all)
{

  if (process_id == -1)
  {
    if (all)
      sendsignal2devices(SIGTERM);
    remove_pid(pidfile);
    if (*infofile)
      unlink(infofile);
    writelogfile(LOG_CRIT, 1, "Smsd mainprocess terminated abnormally. PID: %i.", (int)getpid());

    flush_smart_logging();
    closelogfile();

#ifndef NOSTATS
    MM_destroy();
#endif
    exit(EXIT_FAILURE);
  }
  else if (process_id >= 0)
  {
    if (all)
      kill((int)getppid(), SIGTERM);

    writelogfile(LOG_CRIT, 1, "Modem handler %i terminated abnormally. PID: %i.", process_id, (int)getpid());

    flush_smart_logging();
    closelogfile();

    exit(EXIT_FAILURE);
  }
}

void signal_handler(int signum)
{
  signal(SIGCONT,SIG_IGN);
  signal(SIGUSR2,SIG_IGN);

  if (signum == SIGCONT)
  {
    if (process_id == -1)
    {
      // 3.1.2: Signal handlers are now silent.

#ifdef DEBUG_SIGNALS_NOT_FOR_PRODUCTION
      writelogfile(LOG_CRIT, 0, "Smsd mainprocess received SIGCONT, will continue %s. PID: %i.",
                   (workless_delay)? "immediately" : "without delays", (int)getpid());
#endif

      sendsignal2devices(SIGCONT);
      break_workless_delay = 1;
    }
    else if (process_id >= 0)
    {
#ifdef DEBUG_SIGNALS_NOT_FOR_PRODUCTION
      writelogfile(LOG_INFO, 0, "Modem handler %i received SIGCONT, will continue %s. PID: %i.",
                   process_id, (workless_delay)? "immediately" : "without delays", (int)getpid());
#endif

      break_workless_delay = 1;
    }
  }
  else if (signum == SIGUSR2)
    break_suspend = 1;

  signal(SIGCONT,signal_handler);
  signal(SIGUSR2,signal_handler);
}

/* =======================================================================
   Main
   ======================================================================= */

int main(int argc,char** argv)
{
  int i;
  struct passwd *pwd;
  struct group *grp;
  int result = 1;
  pid_t pid;
  char timestamp[81];

  terminate = 0;
  strcpy(process_title, "smsd");
  signal(SIGTERM,soft_termination_handler);
  signal(SIGINT,soft_termination_handler);
  signal(SIGHUP,soft_termination_handler);
  signal(SIGUSR1,soft_termination_handler);
  signal(SIGUSR2,signal_handler);
  signal(SIGCONT,signal_handler);
  // TODO: Some more signals should be ignored or handled too? 
  *run_info = 0;
  incoming_pdu_store = NULL;
  outgoing_pdu_store = NULL;
  routed_pdu_store = NULL;
  getfile_err_store = NULL;
  check_memory_buffer = NULL;
  check_memory_buffer_size = 0;
  for (i = 0; i < NUMBER_OF_MODEMS; i++)
    device_pids[i] = 0;
  parsearguments(argc,argv);
  initcfg();
  if (!readcfg())
    exit(EXIT_FAILURE);

	if (do_encode_decode_arg_7bit_packed)
	{
		char buffer[512];

#ifdef USE_ICONV
		if (!iconv_init())
			exit(EXIT_FAILURE);
#endif
		if (do_encode_decode_arg_7bit_packed == 1)
			encode_7bit_packed(arg_7bit_packed, buffer, sizeof(buffer));
		else
			decode_7bit_packed(arg_7bit_packed, buffer, sizeof(buffer));

		printf("%s\n", buffer);

#ifdef DEBUGMSG
		if (do_encode_decode_arg_7bit_packed == 1)
		{
			strcpy(arg_7bit_packed, buffer);
			decode_7bit_packed(arg_7bit_packed, buffer, sizeof(buffer));
			printf("back:\n");
			printf("%s\n", buffer);
		}
#endif
		exit(0);
	}

  // Command line overrides smsd.conf settings:
  if (*arg_infofile)
    strcpy(infofile, arg_infofile);
  if (*arg_pidfile)
    strcpy(pidfile, arg_pidfile);
  if (*arg_logfile)
    strcpy(logfile, arg_logfile);
  if (*arg_username)
    strcpy(username, arg_username);
  if (*arg_groupname)
    strcpy(groupname, arg_groupname);
  if (arg_terminal == 1)
    terminal = 1;

	// 3.1.7: If group was given, add that to the group access list (previously was set to only group).
	if (getuid() == 0 && *username && strcmp(username, "root"))
	{
		if (!(pwd = getpwnam(username)))
		{
			fprintf(stderr, "User %s not found.\n", username);
			result = 0;
		}
		else
		{
			gid_t gt = pwd->pw_gid;

			if (*groupname)
			{
				if (!(grp = getgrnam(groupname)))
				{
					fprintf(stderr, "Group %s not found.\n", groupname);
					result = 0;
				}
				else
					gt = grp->gr_gid;
			}

			if (result)
			{
				if (setgid(gt))
				{
					fprintf(stderr, "Unable to setgid to %i. errno %i\n", (int) gt, errno);
					result = 0;
				}
				else if (initgroups(pwd->pw_name, gt))
				{
					fprintf(stderr, "Unable to initgroups for user id %i (%s).\n", (int) pwd->pw_uid, username);
					result = 0;
				}
			}

			if (result && setuid(pwd->pw_uid))
			{
				fprintf(stderr, "Error setting the user id %i (%s).\n", (int) pwd->pw_uid, username);
				result = 0;
			}
		}
	}

#if 0
	if (result)
	{
		gid_t groupIDs[NGROUPS_MAX];
		int i, count;

		pwd = getpwuid(getuid());
		grp = getgrgid(getgid());
		if (pwd && grp)
			fprintf(stderr, "Running as %s:%s\n", pwd->pw_name, grp->gr_name);

		if ((count = getgroups(NGROUPS_MAX, groupIDs)) == -1)
			perror("getgroups error");
		else
		{
			for (i = 0; i < count; i++)
			{
				grp = getgrgid(groupIDs[i]);
				printf("Group ID #%d: %d %s\n", i + 1, (int) groupIDs[i], grp->gr_name);
			}
		}

		exit(0);
	}
#endif

  if (result == 0)
  {
    if (startup_err_str)
      free(startup_err_str);
    exit(EXIT_FAILURE);
  }

  logfilehandle = openlogfile(logfile, LOG_DAEMON, loglevel);  
  writelogfile(LOG_CRIT, 0, "Smsd v%s started.", smsd_version);  

  // 3.1.9: Change the current working directory
  if (chdir("/") < 0)
  {
    char *p = "Unable to change the current working directory to \"/\".";

    fprintf(stderr, "%s\n", p);
    writelogfile(LOG_CRIT, 0, p);
    exit(EXIT_FAILURE);
  }

  pwd = getpwuid(getuid());
  grp = getgrgid(getgid());
  if (pwd && grp)
    writelogfile(LOG_CRIT, 0, "Running as %s:%s.", pwd->pw_name, grp->gr_name);

  if (strstr(smsd_version, "beta"))
  {
    writelogfile(LOG_CRIT, 0, "# You are running a beta version of SMS Server Tools 3.");
    writelogfile(LOG_CRIT, 0, "# All feedback is valuable.");
    writelogfile(LOG_CRIT, 0, "# Please provide you feedback on SMSTools3 Community. Thank you.");
  }

  if (startup_check(read_translation()) > 0)
  {
    writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated.");
    exit(EXIT_FAILURE);
  }

  if (strcmp(datetime_format, DATETIME_DEFAULT))
  {
    make_datetime_string(timestamp, sizeof(timestamp), 0, 0, 0);
    writelogfile(LOG_INFO, 0, "Using datetime format \"%s\". It produces \"%s\".", datetime_format, timestamp);
  }

  if (strcmp(logtime_format, LOGTIME_DEFAULT))
  {
    make_datetime_string(timestamp, sizeof(timestamp), 0, 0, logtime_format);
    writelogfile(LOG_INFO, 0, "Using logtime format \"%s\". It produces \"%s\".", logtime_format, timestamp);
  }

  if (strcmp(date_filename_format, DATE_FILENAME_DEFAULT))
  {
    make_datetime_string(timestamp, sizeof(timestamp), 0, 0, date_filename_format);
    writelogfile(LOG_INFO, 0, "Using date_filename format \"%s\". It produces \"%s\".", date_filename_format, timestamp);
  }

  // 3.1.5: Shared memory is created after main process is running:
  //initstats();
  //loadstats();

#ifdef TERMINAL
  terminal = 1;
#endif
#ifdef DEBUGMSG
  terminal = 1;
#endif
  if (strcmp(logfile, "1") == 0 || strcmp(logfile, "2") == 0)
    terminal = 1;
  if (printstatus)
    terminal = 1;
  if (*communicate)
    terminal = 1;
  if (terminal)
    writelogfile(LOG_CRIT, 0, "Running in terminal mode.");
  else
  {
    i = fork();
    if (i < 0)
    {
      writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated because of the fork() failure.");
      exit(EXIT_FAILURE);
    }

    if (i > 0)
      exit(EXIT_SUCCESS);

    i = setsid();
    if (i < 0)
    {
      writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated because of the setsid() failure.");
      exit(EXIT_FAILURE);
    }
  }

#ifdef USE_ICONV
  if (!iconv_init())
  {
    writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated because of the iconv_open() failure.");
    exit(EXIT_FAILURE);
  }
#endif

  time(&process_start_time);

  if (write_pid(pidfile) == 0)
  {
    fprintf(stderr, "Smsd mainprocess terminated because the pid file %s cannot be written.\n", pidfile);
    writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated because the pid file %s cannot be written.", pidfile);
    exit(EXIT_FAILURE);
  }

  if (!terminal)
  {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);
  }

  if (*communicate)
  {
    for (i = 0; i < NUMBER_OF_MODEMS; i++)
      if (strcmp(communicate, devices[i].name) == 0)
        break;
    if (i >= NUMBER_OF_MODEMS)
    {
      fprintf(stderr, "Unable to talk with %s, device not found.\n", communicate);
      writelogfile(LOG_CRIT, 0, "Unable to talk with %s, device not found.", communicate);

      // 3.1:
      exit(EXIT_FAILURE);
    }
  }

  // 3.1.5: Create shared memory now, filename gets pid of mainprocess:
  initstats();
  loadstats();

  if (!(*communicate) && keep_messages)
    writelogfile(LOG_CRIT, 0, "This is a test run: messages are kept and smsd will stop after reading.");

  // 3.1.7:
  if (use_linux_ps_trick)
  {
    // Make readable process name
    for (i = 0; i < argc; i++)
      memset(argv[i], 0, strlen(argv[i]));
    strcpy(argv[0], "smsd: MAINPROCESS");
  }

  // Start sub-processes for each modem
  for (i = 0; i < NUMBER_OF_MODEMS; i++)
  {
    if (devices[i].name[0])
    {
      // 3.1: If talking with one modem, other threads are not started:
      if (*communicate)
        if (strcmp(communicate, devices[i].name) != 0)
          continue;

      pid = fork();
      if (pid > 0)
        device_pids[i] = pid;
      if (pid == 0)
      {
        if (use_linux_ps_trick)
        {
          memset(argv[0] + sizeof("smsd: ") - 1, 0, sizeof("MAINPROCESS") - 1);
          strcpy(argv[0] + sizeof("smsd: ") - 1, devices[i].name);
        }
        else
        {
          // 3.1.4: 
          int idx;

          for (idx = 0; idx < argc; idx++)
          {
            if (strncmp(argv[idx], "MAINPROCESS", 11) == 0)
            {
              size_t l = strlen(devices[i].name);

              if (l > strlen(argv[idx]))
                l = strlen(argv[idx]);
              strncpy(argv[idx], devices[i].name, l);
              while (argv[idx][l])
                argv[idx][l++] = '_';
              break;
            }
            else if (!strncmp(argv[idx], "-nMAINPROCESS", 13))
            {
               size_t l = strlen(devices[i].name);

               if (l > strlen(argv[idx]) - 2)
                 l = strlen(argv[idx] - 2);
               strncpy(argv[idx] + 2, devices[i].name, l);
               while (argv[idx][l + 2])
                 argv[idx][l++ + 2] = '_';
               break;
            }
          }
        }
        // ------

        time(&process_start_time);

        process_id = i;
        strcpy(process_title, DEVICE.name);

        if (strcmp(communicate, process_title) == 0)
        {
          if (DEVICE.logfile[0])
          {
            if (DEVICE.loglevel != -1)
              loglevel = DEVICE.loglevel;
            logfilehandle = openlogfile(DEVICE.logfile, LOG_DAEMON, loglevel);  
          }

          if (talk_with_modem() == 0)
            writelogfile(LOG_CRIT, 0, "Unable to talk with modem.");
        }
        else
        {
          modem_handle = -1;

          // 3.1.?:
          if (DEVICE.conf_identity[0] && DEVICE.modem_disabled == 0)
          {
            if (try_openmodem())
            {
              char answer[500];
              char *p;
              int retries = 0;

              writelogfile(LOG_INFO, 0, "Checking if modem in %s is ready", DEVICE.device);
              do
              {
                retries++;
                put_command("AT\r", answer, sizeof(answer), 1, EXPECT_OK_ERROR);
                if (!strstr(answer, "OK") && !strstr(answer, "ERROR"))
                {
                  if (terminate)
                    break;

                  // if Modem does not answer, try to send a PDU termination character
                  put_command("\x1A\r", answer, sizeof(answer), 1, EXPECT_OK_ERROR);

                  if (terminate)
                    break;
                }
              }
              while (retries <= 5 && !strstr(answer,"OK"));
              if (!strstr(answer,"OK"))
              {
                p = get_gsm_error(answer);
                writelogfile0(LOG_ERR, 1, tb_sprintf("Modem is not ready to answer commands%s%s. Stopping.", (*p)? ", " : "", p));
                //alarm_handler0(LOG_ERR, tb);
                exit(127);
              }

              put_command("AT+CIMI\r", answer, SIZE_IDENTITY, 1,EXPECT_OK_ERROR);

              while (*answer && !isdigitc(*answer))
                strcpyo(answer, answer +1);

              if (strstr(answer, "ERROR"))
              {
                put_command("AT+CGSN\r", answer, SIZE_IDENTITY, 1, EXPECT_OK_ERROR);

                while (*answer && !isdigitc(*answer))
                  strcpyo(answer, answer +1);
              }

              try_closemodem(1);

              if (!strstr(answer, "ERROR"))
              {
                if ((p = strstr(answer, "OK")))
                  *p = 0;
                cut_ctrl(answer);
                cutspaces(answer);

                if (!strcmp(DEVICE.conf_identity, answer))
                  writelogfile(LOG_INFO, 0, "Checking identity: OK.");
                else
                {
                  int n;
                  int found = 0;

                  writelogfile(LOG_INFO, 0, "Checking identity: No match, searching new settings.");

                  for (n = 0; n < NUMBER_OF_MODEMS; n++)
                  {
                    if (devices[n].name[0])
                    {
                      if (!strcmp(devices[n].conf_identity, answer))
                      {
                        writelogfile(LOG_INFO, 0, "Applying new settings, continuing as %s, process_id %i --> %i.", devices[n].name, process_id, n);
                        strcpyo(devices[n].device, DEVICE.device);
                        process_id = n;
                        strcpy(process_title, DEVICE.name);
/*
strcpy(process_title, "!");
strcat(process_title, DEVICE.name);
strcpy(DEVICE.name, process_title);
*/
                        found = 1;
                        break;
                      }
                    }
                  }

                  if (!found)
                  {
                    writelogfile(LOG_CRIT, 1, "Did not find new settings. Stopping.");
                    exit(127);
                  }
                }
              }
              else
              {
                writelogfile(LOG_CRIT, 1, "Cannot check identity. Stopping.");
                exit(127);
              }
            }
          }

          if (DEVICE.logfile[0])
          {
            if (DEVICE.loglevel != -1)
              loglevel = DEVICE.loglevel;
            logfilehandle = openlogfile(DEVICE.logfile, LOG_DAEMON, loglevel);  
          }

          devicespooler();

          send_stopstring();

          try_closemodem(1);
          statistics[i]->status = 'b';
        }

        strftime(timestamp, sizeof(timestamp), datetime_format, localtime(&process_start_time));

        writelogfile(LOG_CRIT, 0, "Modem handler %i terminated. PID: %i, was started %s.", process_id, (int)getpid(), timestamp);

        flush_smart_logging();

        if (DEVICE.logfile[0])
          closelogfile();

        exit(127);
      }
    }
  } 
  // Start main program
  process_id=-1;
  mainspooler();
  writelogfile(LOG_CRIT, 0, "Smsd mainprocess is awaiting the termination of all modem handlers. PID: %i.", (int)getpid());
  waitpid(0,0,0);
  savestats();
#ifndef NOSTATS
  MM_destroy();
#endif
  remove_pid(pidfile);
  if (*infofile)
    unlink(infofile);

  strftime(timestamp, sizeof(timestamp), datetime_format, localtime(&process_start_time));

  writelogfile(LOG_CRIT, 0, "Smsd mainprocess terminated. PID %i, was started %s.", (int)getpid(), timestamp);

  flush_smart_logging();
  closelogfile();

  return 0;
}
