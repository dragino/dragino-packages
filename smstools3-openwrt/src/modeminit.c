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
#include <fcntl.h>
#include <termios.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>
#include "logging.h"
#include "alarm.h"

#ifdef SOLARIS
#include <sys/filio.h>
#include <strings.h> // for bzero().
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifndef DISABLE_INET_SOCKET
// HG
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include "extras.h"
#include "modeminit.h"
#include "smsd_cfg.h"
#include "version.h"
#include "pdu.h"
#include "stats.h"

// Define a dummy if the OS does not support hardware handshake
#ifndef CRTSCTS
#define CRTSCTS 0
#endif

typedef struct {
  int code;
  char *text;
} _gsm_general_error; 

_gsm_general_error gsm_cme_errors[] =
{
  // 3GPP TS 07.07 version 7.8.0 Release 1998 (page 90) ETSI TS 100 916 V7.8.0 (2003-03)
  {0,	"phone failure"},
  {1,	"no connection to phone"},
  {2,	"phone-adaptor link reserved"},
  {3,	"operation not allowed"},
  {4,	"operation not supported"},
  {5,	"PH-SIM PIN required"},
  {6,	"PH-FSIM PIN required"},
  {7,	"PH-FSIM PUK required"},
  {10,	"SIM not inserted"},
  {11,	"SIM PIN required"},
  {12,	"SIM PUK required"},
  {13,	"SIM failure"},
  {14,	"SIM busy"},
  {15,	"SIM wrong"},
  {16,	"incorrect password"},
  {17,	"SIM PIN2 required"},
  {18,	"SIM PUK2 required"},
  {20,	"memory full"},
  {21,	"invalid index"},
  {22,	"not found"},
  {23,	"memory failure"},
  {24,	"text string too long"},
  {25,	"invalid characters in text string"},
  {26,	"dial string too long"},
  {27,	"invalid characters in dial string"},
  {30,	"no network service"},
  {31,	"network timeout"},
  {32,	"network not allowed - emergency calls only"},
  {40,	"network personalisation PIN required"},
  {41,	"network personalisation PUK required"},
  {42,	"network subset personalisation PIN required"},
  {43,	"network subset personalisation PUK required"},
  {44,	"service provider personalisation PIN required"},
  {45,	"service provider personalisation PUK required"},
  {46,	"corporate personalisation PIN required"},
  {47,	"corporate personalisation PUK required"},

  // 3.1.5:
  {515, "device busy"},

  {100,	"unknown"}

// Some other possible errors (source document?):
//CME ERROR: 48  PH-SIM PUK required
//CME ERROR: 256 Operation temporarily not allowed
//CME ERROR: 257 Call barred
//CME ERROR: 258 Phone is busy
//CME ERROR: 259 User abort
//CME ERROR: 260 Invalid dial string
//CME ERROR: 261 SS not executed
//CME ERROR: 262 SIM Blocked

};

_gsm_general_error gsm_cms_errors[] =
{
  // Table 8.4/GSM 04.11 (part 1):
  {1,	"Unassigned (unallocated) number"},
  {8,	"Operator determined barring"},
  {10,	"Call barred"},
  {21,	"Short message transfer rejected"},
  {27,	"Destination out of order"},
  {28,	"Unindentified subscriber"},
  {29,	"Facility rejected"},
  {30,	"Unknown subscriber"},
  {38,	"Network out of order"},
  {41,	"Temporary failure"},
  {42,	"Congestion"},
  {47,	"Recources unavailable, unspecified"},
  {50,	"Requested facility not subscribed"},
  {69,	"Requested facility not implemented"},
  {81,	"Invalid short message transfer reference value"},
  {95,	"Semantically incorrect message"},
  {96,	"Invalid mandatory information"},
  {97,	"Message type non-existent or not implemented"},
  {98,	"Message not compatible with short message protocol state"},
  {99,	"Information element non-existent or not implemented"},
  {111,	"Protocol error, unspecified"},
  {127,	"Internetworking , unspecified"},
  // Table 8.4/GSM 04.11 (part 2):
  {22,	"Memory capacity exceeded"},
  // GSM 03.40 subclause 9.2.3.22 values.
  {128,	"Telematic internetworking not supported"},
  {129,	"Short message type 0 not supported"},
  {130,	"Cannot replace short message"},
  {143,	"Unspecified TP-PID error"},
  {144,	"Data code scheme (alphabet) not supported"},
  {145,	"Message class not supported"},
  {159,	"Unspecified TP-DCS error"},
  {160,	"Command cannot be actioned"},
  {161,	"Command unsupported"},
  {175,	"Unspecified TP-Command error"},
  {176,	"Transfer Protocol Data Unit (TPDU) not supported"},
  {192,	"Service Center (SC) busy"},
  {193,	"No SC subscription"},
  {194,	"SC System failure"},
  {195,	"Invalid Short Message Entity (SME) address"},
  {196,	"Destination SME barred"},
  {197,	"SM Rejected-Duplicate SM"},
  {198,	"Validity Period Format (TP-VPF) not supported"},
  {199,	"Validity Period) TP-VP not supported"},
  {208,	"SIM SMS Storage full"},
  {209,	"No SMS Storage capability in SIM"},
  {210,	"Error in MS"},
  {211,	"Memory capacity exceeded"},
  {212,	"Sim Application Toolkit busy"},
  {213,	"SIM data download error"},
  {255,	"Unspecified error cause"},
  // 3GPP TS 27.005 subclause 3.2.5 values /3/.
  {300,	"ME Failure"},
  {301,	"SMS service of ME reserved"},
  {302,	"Operation not allowed"},
  {303,	"Operation not supported"},
  {304,	"Invalid PDU mode parameter"},
  {305,	"Invalid Text mode parameter"},
  {310,	"(U)SIM not inserted"},
  {311,	"(U)SIM PIN required"},
  {312,	"PH-(U)SIM PIN required"},
  {313,	"(U)SIM failure"},
  {314,	"(U)SIM busy"},
  {315,	"(U)SIM wrong"},
  {316,	"(U)SIM PUK required"},
  {317,	"(U)SIM PIN2 required"},
  {318,	"(U)SIM PUK2 required"},
  {320,	"Memory failure"},
  {321,	"Invalid memory index"},
  {322,	"Memory full"},
  {330,	"SMSC address unknown"},
  {331,	"No network service"},
  {332,	"Network timeout"},
  {340,	"No +CNMA acknowledgement expected"},
  {500,	"Unknown error"},

  // 3.1.5: This error occurs when you try to send a message and the module is receiving another one at the same time. 
  // This causes a collision in the message transfer protocol resulting in failure in sending the SMS.
  // Sometimes, +CMS ERROR: 512 may also occur when the module is receiving weak signal and is loosing connection.
  {512, "MM establishment failure / weak signal, loosing connection"},

  // 3.1.5: ack for 28s after transmission or 42s after channel establishment
  {513, "Lower layer failure: receiving of an acknowledgement timed out or lost the radio link."},

  // 3.1.5:
  {514, "Network error. Congestion in the network."},

  {515, "Please wait, service is not available, init in progress"}
};

char *get_gsm_cme_error(int code)
{
  int i;
  int m = sizeof gsm_cme_errors / sizeof *gsm_cme_errors;

  for (i = 0; i < m; i++)
    if (code == gsm_cme_errors[i].code)
      return gsm_cme_errors[i].text;

  return "";
}

char *get_gsm_cms_error(int code)
{
  int i;
  int m = sizeof gsm_cms_errors / sizeof *gsm_cms_errors;

  for (i = 0; i < m; i++)
    if (code == gsm_cms_errors[i].code)
      return gsm_cms_errors[i].text;

  return "";
}

char *get_gsm_error(char *answer)
{
  char *p;

  if (answer && *answer)
  {
    if ((p = strstr(answer, "+CME ERROR: ")))
      return get_gsm_cme_error(atoi(p +12));
    if ((p = strstr(answer, "+CMS ERROR: ")))
      return get_gsm_cms_error(atoi(p +12));
  }

  return "";
}

char *explain_csq_buffer(char *buffer, int short_form, int ssi, int ber, int signal_quality_ber_ignore)
{

  strcpy(buffer, (short_form)? "ssi: " : "Signal Strength Indicator: ");
  if (ssi == 99 || ssi > 31 || ssi < 0)
    strcat(buffer, (short_form)? "??" : "not present of not measurable");
  else
  {
    // 3.1.12: explain level:
    int dbm;
    char *level = "";

    dbm = -113 + 2 * ssi;

    if (dbm <= -95)
      level = " (Marginal)"; // Marginal - Levels of -95dBm or lower.
    else if (dbm <= -85)
      level = " (Workable)"; // Workable under most conditions - Levels of -85dBm to -95dBm.
    else if (dbm <= -75)
      level = " (Good)"; // Good - Levels between -75dBm and -85dBm.
    else
      level = " (Excellent)"; // Excellent - levels above -75dBm.

    if (short_form)
      sprintf(strchr(buffer, 0), "%i dBm%s", dbm, level);
    else
      sprintf(strchr(buffer, 0), "(%d,%d) %i dBm%s%s", ssi, ber, dbm, level, (ssi == 0)? " or less" : "");
  }

  if (!signal_quality_ber_ignore)
  {
    strcat(buffer, (short_form)? ", ber: " : ", Bit Error Rate: ");
    switch (ber)
    {
      case 0:
        strcat(buffer, (short_form)? "< 0.2 %" : "less than 0.2 %");
        break;

      case 1:
        strcat(buffer, "0.2 - 0.4 %");
        break;

      case 2:
        strcat(buffer, "0.4 - 0.8 %");
        break;

      case 3:
        strcat(buffer, "0.8 - 1.6 %");
        break;

      case 4:
        strcat(buffer, "1.6 - 3.2 %");
        break;

      case 5:
        strcat(buffer, "3.2 - 6.4 %");
        break;

      case 6:
        strcat(buffer, "6.4 - 12.8 %");
        break;

      case 7:
        strcat(buffer, (short_form)? "> 12.8 %" : "more than 12.8 %");
        break;

      default:
        strcat(buffer, (short_form)? "??" : "not known or not detectable");
        break;
    }
  }

  return buffer;
}

void explain_csq(int loglevel, int short_form, char *answer, int signal_quality_ber_ignore)
{
  int ssi;
  int ber = 99;
  char *p;
  char buffer[1024];

  if (strstr(answer, "ERROR"))
    return;

  // 3.1.12: Allow "echo on":
  //if (strncmp(answer, "+CSQ:", 5))
  //  return;
  if (!(p = strstr(answer, "+CSQ:")))
    return;

  //ssi = atoi(answer +5);
  ssi = atoi(p +5);
  //if ((p = strchr(answer, ',')))
  if ((p = strchr(p, ',')))
    ber = atoi(p +1);

  explain_csq_buffer(buffer, short_form, ssi, ber, signal_quality_ber_ignore);

  writelogfile0(loglevel, 0, buffer);
}

int write_to_modem(char *command, int timeout, int log_command, int print_error)
{
  int status=0;
  int timeoutcounter=0;
  int x=0;
  struct termios tio;

  if (command && command[0])
  {
    if (log_command)
      writelogfile(LOG_DEBUG, 0, "-> %s",command);

    // 3.1.9:
    if (DEVICE.send_handshake_select)
    {
      size_t r = 0, bs, n;
      ssize_t got;
      fd_set writefds;

      n = strlen(command);
      while (n > 0)
      {
        bs = (DEVICE.send_delay < 1) ? n : 1;
        got = write(modem_handle, command + r, bs);
        if (got < 0)
        {
          if (errno == EINTR)
            continue;

          if (errno != EAGAIN)
          {
            writelogfile0(LOG_ERR, 1, tb_sprintf("write_to_modem: error %d: %s", errno, strerror(errno)));
            alarm_handler0(LOG_ERR, tb);
            return 0;
          }

          writelogfile0(LOG_DEBUG, 0, tb_sprintf("write_to_modem: device busy, waiting"));
          alarm_handler0(LOG_DEBUG, tb);
          FD_ZERO(&writefds);
          FD_SET(modem_handle, &writefds);
          select(modem_handle + 1, NULL, &writefds, NULL, NULL);
          continue;
        }

        n -= got;
        r += got;

        if (DEVICE.send_delay > 0)
          usleep_until(time_usec() + DEVICE.send_delay * 1000);

        tcdrain(modem_handle);
      }
    }
    else
    {
      tcgetattr(modem_handle, &tio);

      if (!DEVICE_IS_SOCKET && tio.c_cflag & CRTSCTS)
      {
        ioctl(modem_handle, TIOCMGET, &status);
        while (!(status & TIOCM_CTS))
        {
          usleep_until(time_usec() + 100000);
          timeoutcounter++;
          ioctl(modem_handle, TIOCMGET, &status);
          if (timeoutcounter>timeout)
          {
            if (print_error)
              printf("\nModem is not clear to send.\n");
            else
            {
              writelogfile0(LOG_ERR, 1, tb_sprintf("Modem is not clear to send"));
              alarm_handler0(LOG_ERR, tb);
            }
            return 0;
          }
        }
      }

      // 3.1.5:
      if (DEVICE.send_delay < 1)
      {
        if ((size_t)write(modem_handle, command, strlen(command)) != strlen(command))
        {
          writelogfile0(LOG_ERR, 1, tb_sprintf("Could not send string, cause: %s", strerror(errno)));
          alarm_handler0(LOG_ERR, tb);
          return 0;
        }

        if (DEVICE.send_delay < 0)
          tcdrain(modem_handle);
      }
      else
      {
        for(x=0;(size_t)x<strlen(command);x++)
        {
          if (write(modem_handle, command +x, 1) < 1)
          {
            if (print_error)
              printf("\nCould not send character %c, cause: %s\n",command[x],strerror(errno));
            else
            {
              writelogfile0(LOG_ERR, 1, tb_sprintf("Could not send character %c, cause: %s", command[x], strerror(errno)));
              alarm_handler0(LOG_ERR, tb);
            }
            return 0;
          }
          usleep_until(time_usec() + DEVICE.send_delay *1000);
          tcdrain(modem_handle);
        }
      }
    }
  }

  return 1;
}

// 3.1.12:
void negotiate_with_telnet(char *answer, int *len)
{
#define IAC  '\xFF' /* Interpret as command: */
#define DONT '\xFE' /* You are not to use option */
#define DO   '\xFD' /* Please, you use option */
#define WONT '\xFC' /* I won't use option */
#define WILL '\xFB' /* I will use option */
#define SB   '\xFA' /* Interpret as subnegotiation */
#define SE   '\xF0' /* End sub negotiation */

  char *title = "Telnet";
  int idx;
  int i;
  int count;
  char response[128];
  char command;
  int got_option;
  char option;

  idx = 0;
  while (idx < *len)
  {
    if (answer[idx] == IAC && idx +1 < *len)
    {
      *response = 0;
      count = 3;
      command = answer[idx +1];
      got_option = idx +2 < *len;
      option = (got_option)? answer[idx +2] : 0;

      if (command == DO && got_option)
      {
        snprintf(response, sizeof(response), "%c%c%c", IAC, WONT, option);
        writelogfile(LOG_DEBUG, 0, "%s: Got DO for %i (0x%02X), answering WONT.", title, (int)option, option);
      }
      else if (command == WILL && got_option)
      {
        snprintf(response, sizeof(response), "%c%c%c", IAC, DONT, option);
        writelogfile(LOG_DEBUG, 0, "%s: Got WILL for %i (0x%02X), answering DONT.", title, (int)option, option);
      }
      else if (command == WONT && got_option)
      {
        writelogfile(LOG_DEBUG, 0, "%s: Got WONT for %i (0x%02X), ignoring.", title, (int)option, option);
      }
      else if (command == SB && got_option)
      {
        writelogfile(LOG_DEBUG, 0, "%s: Got SB for %i (0x%02X), ignoring.", title, (int)option, option);
        count = 4;
      }
      else if (command == SE)
      {
        writelogfile(LOG_DEBUG, 0, "%s: Got SE, ignoring.", title);
        count = 2;
      }

      if (*response)
        if ((size_t)write(modem_handle, response, strlen(response)) != strlen(response))
          writelogfile(LOG_ERR, 1, "%s: Failed to send response.", title);

      for (i = idx; i +count < *len; i++)
        answer[i] = answer[i +count];

      *len -= count;
    }
    else
      idx++;
  }

  answer[*len] = 0;

  *response = 0;
  if (DEVICE.telnet_login_prompt[0] && strstr(answer, DEVICE.telnet_login_prompt) && DEVICE.telnet_login[0])
  {
    if (DEVICE.telnet_login_prompt_ignore[0] == 0 || !strstr(answer, DEVICE.telnet_login_prompt_ignore))
      snprintf(response, sizeof(response), "%s\n", DEVICE.telnet_login);
  }
  else if (DEVICE.telnet_password_prompt[0] && strstr(answer, DEVICE.telnet_password_prompt) && DEVICE.telnet_password[0])
    snprintf(response, sizeof(response), "%s\n", DEVICE.telnet_password);

  if (*response)
    if ((size_t)write(modem_handle, response, strlen(response)) != strlen(response))
      writelogfile(LOG_ERR, 1, "%s: Failed to send response (login/password).", title);

#undef IAC
#undef DONT
#undef DO
#undef WONT
#undef WILL
#undef SB
#undef SE
}

// Read max characters from modem. The function returns when it received at 
// least 1 character and then the modem is quiet for timeout*0.1s.
// The answer might contain already a string. In this case, the answer 
// gets appended to this string.
int read_from_modem(char *answer, int max, int timeout)
{
  int count=0;
  int got=0;
  int timeoutcounter=0;
  int success=0;
  int toread=0;
  
  // Cygwin does not support TIOC functions, so we cannot use it.
  // ioctl(modem,FIONREAD,&available);	// how many bytes are available to read?

  do 
  {
    // How many bytes do I want to read maximum? Not more than buffer size -1 for termination character.
    count=strlen(answer);
    toread=max-count-1;
    if (toread<=0)
      break;
    // read data
    got = read(modem_handle, answer +count, toread);
    // if nothing received ...
    if (got<=0)
    {
      // wait a litte bit and then repeat this loop
      got=0;
      usleep_until(time_usec() + 100000);
      timeoutcounter++;
    }
    else  
    {
      // 3.1.9
#ifdef DEBUGMSG
      log_read_from_modem = 1;
#endif
      if (log_read_from_modem)
      {
        char tmp[SIZE_LOG_LINE];
        int i;

        snprintf(tmp, sizeof(tmp), "read_from_modem: count=%i, got=%i:", count, got);
        for (i = count; i < count + got; i++)
        {
          if (strlen(tmp) >= sizeof(tmp) - 6)
          {
            strcpy(tmp, "ERROR: too much data");
            break;
          }

          sprintf(strchr(tmp, 0), " %02X[%c]", (unsigned char) answer[i], ((unsigned char) answer[i] >= ' ') ? answer[i] : '.');
        }

        writelogfile(LOG_CRIT, 0, tmp);
      }

      // restart timout counter
      timeoutcounter=0;
      // append a string termination character
      answer[count+got]=0;
      success=1;      

      // 3.1.12: With Multitech network modem (telnet) there can be 0x00 inside the string:
      if (strlen(answer) < (size_t)count + got)
      {
        int i, j, len;

        len = count + got;
        j = 0;
        for (i = 0; i < count + got; i++)
        {
          if (answer[i] == '\0')
          {
            len--;
            continue;
          }

          if (i > j)
            answer[j] = answer[i];
          j++;
        }

        answer[len] = 0;
      }

    }
  }
  while (timeoutcounter < timeout);

  // 3.1.12:
  if (success && DEVICE_IS_SOCKET)
  {
    count += got;
    negotiate_with_telnet(answer, &count);
  }

  return success;
}

// 3.1.1:
char *change_crlf(char *str, char ch)
{
  char *p;

  while ((p = strchr(str, '\r')))
    *p = ch;
  while ((p = strchr(str, '\n')))
    *p = ch;

  return str;
}

int detect_routed_message(char *answer)
{
  int result = 0;
  // keywords must have same length:
  int keyword_length = 5;
  char *keyword    = "+CMT:";
  char *keyword_sr = "+CDS:";
  char *p;
  char *term;
  int pdu_length;
  int can_handle;
  int is_sr;
  int send_ack = 0;
  char *p1;
  char *p2;

  if (*answer)
  {
    // We can have answer which contains only routed message, or there can be answer like:
    // +CPMS: "SM",0,20,"SM",0,20,"SM",0,20 OK ... +CMT: ,59 07915348150110...

    is_sr = 0;
    if (!(p1 = strstr(answer, keyword)))
      if ((p1 = strstr(answer, keyword_sr)))
        is_sr = 1;

    if (p1)
    {
      if (!is_sr || !DEVICE.using_routed_status_report)
        writelogfile(LOG_ERR, DEVICE.unexpected_input_is_trouble, "Routed %s detected:\n%s", (is_sr) ? "status report" : "message", p1);

      if (DEVICE.routed_status_report_cnma)
        send_ack = 1;

      while (p1)
      {
        result++;
        can_handle = 0;

        if ((term = strchr(p1, '\r')))
        {
          p = term +1;
          if (*p == '\n')
            p++;
          if (octet2bin(p) >= 0)
          {
            if ((term = strchr(p, '\r')))
            {
              // Original answer remains intact. Count the length and check that PDU does not contain delimiter character:
              pdu_length = (int)(term - p);
              p2 = strchr(p, ',');
              if (!p2 || p2 > term)
              {
                // PDU is handled later. If it has some errors, explanation will be printed to the message file.

                if (!routed_pdu_store)
                {
                  if ((routed_pdu_store = (char *)malloc(pdu_length +2)))
                    *routed_pdu_store = 0;
                }
                else
                  routed_pdu_store = (char *)realloc((void *)routed_pdu_store, strlen(routed_pdu_store) +pdu_length +2);

                if (routed_pdu_store)
                {
                  // For easier handling, delimiter in routed_pdu_store is '\n'.
                  sprintf(strchr(routed_pdu_store, 0), "%.*s\n", pdu_length, p);
                  can_handle = 1;
                }
              }
            }
          }
        }

        p = (is_sr)? "status report" : "message";
        if (can_handle)
        {
          if (is_sr && DEVICE.using_routed_status_report)
            writelogfile0(LOG_INFO, 0, tb_sprintf("Saved routed %s for later handling.", p));
          else
          {
            writelogfile0(LOG_ERR, 0, tb_sprintf("Saved routed %s for later handling. However, you MUST DISABLE %s routing with modem settings.", p, p));
            alarm_handler0(LOG_ERR, tb);
          }
        }
        else
        {
          writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot handle this routed %s. You MUST DISABLE %s routing with modem settings.", p, p));
          alarm_handler0(LOG_ERR, tb);
        }

        is_sr = 0;
        p = p1 +keyword_length;
        if (!(p1 = strstr(p, keyword)))
          if ((p1 = strstr(p, keyword_sr)))
            is_sr = 1;
      }
    }

    // TODO: more than one ack if more than one messsage received?
    if (send_ack)
    {
      char loganswer[1024];

      writelogfile(LOG_DEBUG, 0, "Sending acknowledgement");
      write_to_modem("AT+CNMA\r", 30, 1, 0);

      *loganswer = 0;
      read_from_modem(loganswer, sizeof(loganswer), 2);
      if (log_single_lines)
        change_crlf(loganswer, ' ');

      writelogfile(LOG_DEBUG, 0, "<- %s", loganswer);
    }
  }

  return result;
}

void do_hangup(char *answer)
{

	if (DEVICE.hangup_incoming_call == 1 || (DEVICE.hangup_incoming_call == -1 && hangup_incoming_call == 1) || DEVICE.phonecalls == 2)
	{
		char *command = "AT+CHUP\r";
		char tmpanswer[1024];
		int timeoutcounter;

		if (DEVICE.voicecall_hangup_ath == 1 || (DEVICE.voicecall_hangup_ath == -1 && voicecall_hangup_ath == 1))
			command = "ATH\r";

		writelogfile(LOG_NOTICE, 0, "Ending incoming call: %s", answer);

		write_to_modem(command, 30, 1, 0);

		timeoutcounter = 0;
		*tmpanswer = 0;

		do
		{
			read_from_modem(tmpanswer, sizeof(tmpanswer), 2);	// One read attempt is 200ms

			// Any answer is ok:
			if (*tmpanswer)
				break;

			timeoutcounter++;;
		}
		while (timeoutcounter < 5);

		if (!log_unmodified)
		{
			cutspaces(tmpanswer);
			cut_emptylines(tmpanswer);

			if (log_single_lines)
				change_crlf(tmpanswer, ' ');
		}
		writelogfile(LOG_DEBUG, 0, "<- %s", tmpanswer);

		if (DEVICE.communication_delay > 0)
			usleep_until(time_usec() + DEVICE.communication_delay * 1000);
	}
}

int handlephonecall_clip(char *answer)
{
	int result = 0;
	char *p, *e_start, *e_end;
	int len;
	char entry_number[SIZE_PB_ENTRY];
	int entry_type;

	if (DEVICE.phonecalls != 2)
		return 0;

	*entry_number = 0;
	entry_type = 0;

	if ((p = strstr(answer, "+CLIP:")))
	{
		do_hangup(answer);
		result = -1;

		if ((e_start = strchr(p, '"')))
		{
			e_start++;
			if ((e_end = strchr(e_start, '"')))
			{
		                if ((len = e_end -e_start) < SIZE_PB_ENTRY)
				{
					sprintf(entry_number, "%.*s", len, e_start);
					cutspaces(entry_number);
					if (*entry_number == '+')
						strcpyo(entry_number, entry_number +1);

					if (strlen(e_end) >= 3)
					{
						e_end += 2;
						writelogfile(LOG_INFO, 0, "Got phonecall from %s", entry_number);
						savephonecall(entry_number, atoi(e_end), "");
						result = 1;
					}
				}
			}
		}

		if (result == -1)
		{
			writelogfile0(LOG_ERR, 1, tb_sprintf("Error while trying to handle +CLIP."));
			alarm_handler0(LOG_ERR, tb);
		}
	}

	return result;
}

// 3.1beta7: Not waiting any answer if answer is NULL. Return value is then 1/0.
// 3.1.5: In case of timeout return value is -2.

int put_command(char *command, char *answer, int max, int timeout_count, char *expect)
{

  return put_command0(command, answer, max, timeout_count, expect, 0);
}

int put_command0(char *command, char *answer, int max, int timeout_count, char *expect, int silent)
{
  char loganswer[SIZE_LOG_LINE];
  int timeoutcounter = 0;
  regex_t re;
  int got_timeout = 0;
  int regex_allocated = 0;
  int timeout;
  int i;
  static unsigned long long last_command_ended = 0;
  int last_length;

  if (DEVICE.communication_delay > 0)
    if (last_command_ended)
      usleep_until(last_command_ended +DEVICE.communication_delay *1000);

  // read_timeout is in seconds.
  timeout = DEVICE.read_timeout *10 *timeout_count;

  // compile regular expressions
  if (expect && expect[0])
  {
    if (regcomp(&re, expect, REG_EXTENDED|REG_NOSUB) != 0)
    {	  
      fprintf(stderr, "Programming error: Expected answer %s is not a valid regepr\n", expect);
      writelogfile(LOG_CRIT, 1, "Programming error: Expected answer %s is not a valid regepr", expect);
      exit(1);
    }
    regex_allocated = 1;
  }  

  // 3.1.5: Detect and handle routed message. Detect unexpected input.
  if ((DEVICE.incoming && DEVICE.detect_message_routing) || DEVICE.detect_unexpected_input)
  {
    *loganswer = 0;
    do
    {
      i = strlen(loganswer);
      read_from_modem(loganswer, sizeof(loganswer), 2);  // One read attempt is 200ms
    }
    while (strlen(loganswer) > (size_t)i);

    i = 0;
    if (DEVICE.incoming && DEVICE.detect_message_routing)
      i = detect_routed_message(loganswer);

    if (!i && *loganswer && DEVICE.detect_unexpected_input)
    {
      if (!log_unmodified)
      {
        cutspaces(loganswer);
        cut_emptylines(loganswer);

        if (log_single_lines)
          change_crlf(loganswer, ' ');
      }

      if (*loganswer)
      {
        // Some modems send unsolicited result code even when status report is stored for future
        // reading. This and SMS-DELIVER indication is not logged.
        if (!strstr(loganswer, "+CDSI:") && !strstr(loganswer, "+CMTI:"))
          if (!(strstr(loganswer, "+CLIP:") && DEVICE.phonecalls == 2))
            if (!(strstr(loganswer, "+CREG:") && get_loglevel() >= DEVICE.loglevel_lac_ci)) // 3.1.14.
              writelogfile(LOG_ERR, DEVICE.unexpected_input_is_trouble, "Unexpected input: %s", loganswer);

	if (handlephonecall_clip(loganswer) != 1)
          if (strstr(loganswer, "RING") && DEVICE.phonecalls != 2)
            do_hangup(loganswer);
      }
    }
  }

  // clean input buffer
  // It seems that this command does not do anything because actually it
  // does not clear the input buffer. 
  tcflush(modem_handle, TCIFLUSH);
  
  // send command
  if (write_to_modem(command, 30, 1, 0) == 0)
  {
    t_sleep(errorsleeptime);
    // Free memory used by regexp
    if (regex_allocated)
      regfree(&re);

    last_command_ended = time_usec();

    return 0;
  }
 
  if (!answer)
  {
    if (!silent)
      writelogfile(LOG_DEBUG, 0, "Command is sent");
  }
  else
  {
    if (!silent)
      writelogfile(LOG_DEBUG, 0, "Command is sent, waiting for the answer");

    // wait for the modem-answer 
    answer[0] = 0;
    timeoutcounter = 0;
    got_timeout = 1;
    last_length = 0;

    do
    {
      read_from_modem(answer, max, 2);  // One read attempt is 200ms

      // check if it's the expected answer
      if (expect && expect[0] && (regexec(&re, answer, (size_t) 0, NULL, 0) == 0))
      {
        got_timeout = 0;
        put_command_timeouts = 0;
        break;
      }

      // 3.1.1: Some modem does not give "OK" in the answer for CPMS:
      // +CPMS: "SM",0,30,"SM",0,30,"SM",0,30
      if (strstr(answer, "+CPMS:"))
      {
        int i = 0;
        char *p = answer;

        while ((p = strchr(p +1, ',')))
          i++;

        if (i >= 8)
        {
          // 8 commas is enough
          got_timeout = 0;
          put_command_timeouts = 0;
          break;
        }
      }
      // ------------------------------------------------------------

      // 3.1.12: If got something from the modem, do not count timeout:
      //timeoutcounter += 2;
      i = strlen(answer);     
      if (i == last_length)
        timeoutcounter += 2;
      else
      {
        last_length = i;
        timeoutcounter = 0;
      }
    }
    // repeat until timeout
    while (timeoutcounter < timeout);

    if (got_timeout)
    {    
      put_command_timeouts++;
      if (expect && expect[0])
        writelogfile(LOG_DEBUG, 1, "put_command expected %s, timeout occurred. %i.", expect, put_command_timeouts);
    }

    if (DEVICE.incoming && DEVICE.detect_message_routing)
      detect_routed_message(answer);

    // 3.1.5: Some modems (like Westermo GDW-11) start answer with extra <CR><LF>, check and remove it:
    while (!got_timeout && !strncmp(answer, "\r\n", 2))
      strcpyo(answer, answer +2);

    // 3.1.4: error explanation should be included in the answer.
    if (!got_timeout && strstr(answer, "ERROR"))
    {
      char *p;

      p = get_gsm_error(answer);
      if (*p)
        if (max /*sizeof(answer)*/ -strlen(answer) > strlen(p) +3)
          sprintf(strchr(answer, 0), " (%s)", p);
    }

    snprintf(loganswer, sizeof(loganswer), "%s", answer);

    if (!log_unmodified)
    {
      cutspaces(loganswer);
      cut_emptylines(loganswer);

      if (log_single_lines)
        change_crlf(loganswer, ' ');
    }

    writelogfile(LOG_DEBUG, 0, "<- %s", loganswer);

    // 3.1.12: Check if the answer contains a phonecall:
    if (DEVICE.detect_unexpected_input)
    {
      if (handlephonecall_clip(loganswer) != 1)
        if (strstr(loganswer, "RING") && DEVICE.phonecalls != 2)
          do_hangup(loganswer);
    }
  }

  // Free memory used by regexp
  if (regex_allocated)
    regfree(&re);

  last_command_ended = time_usec();

  if (got_timeout)
    return -2;
  if (answer)
    return strlen(answer);
  return 1;
}

void setmodemparams()
{
  struct termios newtio;
  int baudrate;

  if (DEVICE_IS_SOCKET)
    return;

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = CS8 | CLOCAL | CREAD | O_NDELAY | O_NONBLOCK;
  if (DEVICE.rtscts)
    newtio.c_cflag |= CRTSCTS;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;
  newtio.c_cc[VTIME] = 0;
  newtio.c_cc[VMIN] = 0;

  baudrate = DEVICE.baudrate;

  switch (baudrate)
  {
#ifdef B50
    case 50: baudrate = B50; break;
#endif
#ifdef B75
    case 75: baudrate = B75; break;
#endif
#ifdef B110
    case 110: baudrate = B110; break;
#endif
#ifdef B134
    case 134: baudrate = B134; break;
#endif
#ifdef B150
    case 150: baudrate = B150; break;
#endif
#ifdef B200
    case 200: baudrate = B200; break;
#endif
    case 300: baudrate = B300; break;
#ifdef B600
    case 600: baudrate = B600; break;
#endif
    case 1200: baudrate = B1200; break;
#ifdef B1800
    case 1800: baudrate = B1800; break;
#endif
    case 2400: baudrate = B2400; break;
#ifdef B4800
    case 4800: baudrate = B4800; break;
#endif
    case 9600: baudrate = B9600; break;
    case 19200: baudrate = B19200; break;
    case 38400: baudrate = B38400; break;
#ifdef B57600
    case 57600: baudrate = B57600; break;
#endif
#ifdef B115200
    case 115200: baudrate = B115200; break;
#endif
#ifdef B230400
    case 230400: baudrate = B230400; break;
#endif
#ifdef B460800
    case 460800: baudrate = B460800; break;
#endif
#ifdef B500000
    case 500000: baudrate = B500000; break;
#endif
#ifdef B576000
    case 576000: baudrate = B576000; break;
#endif
#ifdef B921600
    case 921600: baudrate = B921600; break;
#endif
#ifdef B1000000
    case 1000000: baudrate = B1000000; break;
#endif
#ifdef B1152000
    case 1152000: baudrate = B1152000; break;
#endif
#ifdef B1500000
    case 1500000: baudrate = B1500000; break;
#endif
#ifdef B2000000
    case 2000000: baudrate = B2000000; break;
#endif
#ifdef B2500000
    case 2500000: baudrate = B2500000; break;
#endif
#ifdef B3000000
    case 3000000: baudrate = B3000000; break;
#endif
#ifdef B3500000
    case 3500000: baudrate = B3500000; break;
#endif
#ifdef B4000000
    case 4000000: baudrate = B4000000; break;
#endif

    default:
      writelogfile(LOG_ERR, 0, "Baudrate %d not supported, using 19200", baudrate);
      baudrate = B19200;
      DEVICE.baudrate = 19200;
  }
  cfsetispeed(&newtio, baudrate);
  cfsetospeed(&newtio, baudrate);
  tcsetattr(modem_handle, TCSANOW, &newtio);
}

int initmodem(char *new_smsc, int receiving)
{
  char command[100];
  char answer[500];
  int retries=0;
  char *p;
  char *pre_initstring = "ATE0+CMEE=1\r";
  char *pre_initstring_clip = "ATE0+CMEE=1;+CLIP=1\r";
  static int reading_checked = 0;

  STATISTICS->last_init = time(0);

  // 3.1beta7: terminating is only checked in case of errors.

  // -----------------------------------------------------------------------------------------------
  writelogfile(LOG_INFO, 0, "Checking if modem is ready");

  // 3.1.7: After being idle, some modems do not answer to the first AT command.
  // With BenQ M32, there can be OK answer, but in many times there is not.
  // To avoid error messages, first send AT and read the answer if it's available.
  if (DEVICE.needs_wakeup_at)
  {
    put_command("AT\r", 0, 0, 1, 0);
    usleep_until(time_usec() + 100000);
    read_from_modem(answer, sizeof(answer), 2);
  }

  retries=0;
  do
  {
    flush_smart_logging();

    retries++;
    put_command("AT\r", answer, sizeof(answer), 1, EXPECT_OK_ERROR);
    if (!strstr(answer, "OK") && !strstr(answer, "ERROR"))
    {
      if (terminate)
        return 7;

      // if Modem does not answer, try to send a PDU termination character
      put_command("\x1A\r", answer, sizeof(answer), 1, EXPECT_OK_ERROR);

      if (terminate)
        return 7;
    }

    // 3.1.7: If it looks like modem does not respond, try to close and open the port:
    if (retries >= 5 && !strstr(answer, "OK"))
    {
      try_closemodem(1);
      t_sleep(1);

      // If open fails, nothing can be done. Error is already logged. Will return 1.
      if (!try_openmodem())
        break;
    }
  }
  while (retries <= 10 && !strstr(answer,"OK"));
  if (!strstr(answer,"OK"))
  {
    // 3.1: more detailed error message:
    p = get_gsm_error(answer);
    // 3.1.5:
    writelogfile0(LOG_ERR, 1, tb_sprintf("Modem is not ready to answer commands%s%s (Timeouts: %i)",
                  (*p)? ", " : "", p, put_command_timeouts));
    alarm_handler0(LOG_ERR, tb);
    return 1;
  }

  // -----------------------------------------------------------------------------------------------
  if (DEVICE.pre_init > 0)
  {
    writelogfile(LOG_INFO, 0, "Pre-initializing modem");

    // 3.1.14:
    //put_command((DEVICE.phonecalls == 2)? pre_initstring_clip : pre_initstring, answer, sizeof(answer), 2, EXPECT_OK_ERROR);
    snprintf(command, sizeof(command), "%s", (DEVICE.phonecalls == 2)? pre_initstring_clip : pre_initstring);
    if (get_loglevel() >= DEVICE.loglevel_lac_ci)
      if (sizeof(command) > strlen(command) +8)
        strcpy(command +strlen(command) -1, ";+CREG=2\r");

    put_command(command, answer, sizeof(answer), 2, EXPECT_OK_ERROR);

    if (!strstr(answer,"OK"))
      writelogfile(LOG_ERR, 1, "Modem did not accept the pre-init string");
  }

  // -----------------------------------------------------------------------------------------------
  // 3.1.1:
  //if (pin[0])
  if (strcasecmp(DEVICE.pin, "ignore"))
  {
    char *cpin_expect = "(READY)|( PIN)|( PUK)|(ERROR)"; // Previously: "(\\+CPIN:.*OK)|(ERROR)"

    writelogfile(LOG_INFO, 0, "Checking if modem needs PIN");
    // 3.1.5: timeout from 50 to 100:
    put_command("AT+CPIN?\r", answer, sizeof(answer), 2, cpin_expect);

    // 3.1.7: Some modems include quotation marks in the answer, like +CPIN: "READY".
    while ((p = strchr(answer, '"')))
      strcpyo(p, p +1);

    // 3.1.12: Allow "echo on":
    if ((p = strstr(answer, "+CPIN:")))
      if (p > answer)
        strcpyo(answer, p);

    // 3.1.7: Some modems may leave a space away after +CPIN:
    if (!strncmp(answer, "+CPIN:", 6) && strncmp(answer, "+CPIN: ", 7))
    {
      if ((p = strdup(answer)))
      {
        snprintf(answer, sizeof(answer), "+CPIN: %s", p + 6);
        free(p);
      }
    }

    if (strstr(answer,"+CPIN: SIM PIN") && !strstr(answer, "PIN2"))
    {
      // 3.1.1:
      if (DEVICE.pin[0] == 0)
        writelogfile(LOG_NOTICE, 1, "Modem needs PIN, but it's not defined for this modem");
      else
      {
        writelogfile(LOG_NOTICE, 0, "Modem needs PIN, entering PIN...");
        sprintf(command, "AT+CPIN=\"%s\"\r", DEVICE.pin);
        put_command(command, answer, sizeof(answer), 6, EXPECT_OK_ERROR);
        if (strstr(answer, "ERROR"))
        {
          p = get_gsm_error(answer);
          writelogfile(LOG_NOTICE, 1, "PIN entering: modem answered %s%s%s",
                       change_crlf(cut_emptylines(cutspaces(answer)), ' '), (*p)? ", " : "", p);
        }
        else
        // After a PIN is entered, some modems need some time before next commands are processed.
        if (DEVICE.pinsleeptime > 0)
        {
          writelogfile(LOG_INFO, 0, "Spending sleep time after PIN entering (%i sec)", DEVICE.pinsleeptime);
          t_sleep(DEVICE.pinsleeptime);
        }

        put_command("AT+CPIN?\r", answer, sizeof(answer), 1, cpin_expect);
        if (strstr(answer,"+CPIN: SIM PIN") && !strstr(answer, "PIN2"))
        {
          writelogfile0(LOG_ERR, 1, tb_sprintf("Modem did not accept this PIN"));
          alarm_handler0(LOG_ERR, tb);
          abnormal_termination(0);
        }
        if (strstr(answer,"+CPIN: READY"))
          writelogfile(LOG_INFO, 0, "PIN Ready");
      }
    }

    if (strstr(answer,"+CPIN: SIM PUK"))
    {
      writelogfile0(LOG_CRIT, 1, tb_sprintf("Your SIM is locked. Unlock it manually"));
      alarm_handler0(LOG_CRIT, tb);
      abnormal_termination(0);
    }

    if (!strstr(answer, "+CPIN: READY"))
    {
      p = get_gsm_error(answer);
      writelogfile0(LOG_CRIT, 1, tb_sprintf("PIN handling: expected READY, modem answered %s%s%s",
        change_crlf(cut_emptylines(cutspaces(answer)), ' '), (*p)? ", " : "", p));
      alarm_handler0(LOG_CRIT, tb);
      abnormal_termination(0);
    }

    // 3.1.1:
    if (DEVICE.pin[0] == 0)
      strcpy(DEVICE.pin, "ignore");
  }

  // -----------------------------------------------------------------------------------------------
  if (DEVICE.initstring[0] || DEVICE.initstring2[0])
    writelogfile(LOG_INFO, 0, "Initializing modem");

  if (DEVICE.initstring[0])
  {
    retries=0;
    do
    {
      retries++;
      put_command(DEVICE.initstring, answer, sizeof(answer), 2, EXPECT_OK_ERROR);
      if (strstr(answer, "ERROR"))
        if (retries < 2)
          if (t_sleep(errorsleeptime))
            return 7;
    }
    while (retries < 2 && !strstr(answer,"OK"));
    if (strstr(answer,"OK")==0)
    {
      // 3.1: more detailed error message:
      p = get_gsm_error(answer);
      writelogfile0(LOG_ERR, 1, tb_sprintf("Modem did not accept the init string%s%s", (*p)? ", " : "", p));
      alarm_handler0(LOG_ERR, tb);
      return 3;
    }
  }

  if (DEVICE.initstring2[0])
  {
    retries=0;
    do
    {
      retries++;
      put_command(DEVICE.initstring2, answer, sizeof(answer), 2, EXPECT_OK_ERROR);
      if (strstr(answer, "ERROR"))
        if (retries < 2)
          if (t_sleep(errorsleeptime))
            return 7;
    }
    while (retries < 2 && !strstr(answer,"OK"));
    if (!strstr(answer,"OK"))
    {
      // 3.1: more detailed error message:
      p = get_gsm_error(answer);
      writelogfile0(LOG_ERR, 1, tb_sprintf("Modem did not accept the second init string%s%s", (*p)? ", " : "", p));
      alarm_handler0(LOG_ERR, tb);
      return 3;
    }
    // 3.1.5:
    else
      explain_csq(LOG_INFO, 0, answer, DEVICE.signal_quality_ber_ignore);
  }

  // -----------------------------------------------------------------------------------------------
  if (DEVICE.status_signal_quality == 1 ||
      (DEVICE.status_signal_quality == -1 && status_signal_quality == 1))
  {
    retries=0;
    do
    {
      retries++;
      put_command("AT+CSQ\r", answer, sizeof(answer), 2, EXPECT_OK_ERROR);
      if (strstr(answer, "ERROR"))
        if (retries < 2)
          if (t_sleep(errorsleeptime))
            return 7;
    }
    while (retries < 2 && !strstr(answer,"OK"));

    // 3.1.12: Allow "echo on":
    //if (!strncmp(answer, "+CSQ:", 5))
    if ((p = strstr(answer, "+CSQ:")))
    {
      //STATISTICS->ssi = atoi(answer +5);
      STATISTICS->ssi = atoi(p +5);
      //if ((p = strchr(answer, ',')))
      if ((p = strchr(p, ',')))
        STATISTICS->ber = atoi(p +1);
      else
        STATISTICS->ber = -2;

      // 3.1.7: Explain signal quality to the log:
      explain_csq(LOG_INFO, 0, answer, DEVICE.signal_quality_ber_ignore);
    }
    else
    {
      STATISTICS->ssi = -2;
      STATISTICS->ber = -2;
    }
  }

  // -----------------------------------------------------------------------------------------------
  // With check_network value 2 network is NOT checked when initializing for receiving:
  if (DEVICE.check_network == 1 ||
      (DEVICE.check_network == 2 && !receiving))
  {
    switch (wait_network_registration(1, 100))
    {
      case -1:
        return 4;
 
      case -2:
        return 7;
    }
  }

  // -----------------------------------------------------------------------------------------------
  writelogfile(LOG_INFO, 0, "Selecting PDU mode");
  strcpy(command,"AT+CMGF=0\r");
  retries=0;
  do
  {
    retries++;
    put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
    if (strstr(answer, "ERROR"))
      if (retries < 2)
        if (t_sleep(errorsleeptime))
          return 7;
  }
  while (retries < 2 && !strstr(answer,"OK"));
  if (strstr(answer,"ERROR"))
  {
    // 3.1: more detailed error message:
    p = get_gsm_error(answer);
    writelogfile0(LOG_ERR, 1, tb_sprintf("Error: Modem did not accept mode selection%s%s", (*p)? ", " : "", p));
    alarm_handler0(LOG_ERR, tb);
    return 5;
  }

  // -----------------------------------------------------------------------------------------------
  if (!DEVICE.smsc_pdu && (new_smsc[0] || DEVICE.smsc[0]))
  {
    writelogfile(LOG_INFO, 0, "Changing SMSC");

    // 3.1.7: clean + character(s) from the setting:
    //sprintf(command, "AT+CSCA=\"+%s\"\r", (new_smsc[0]) ? new_smsc : DEVICE.smsc);
    snprintf(answer, sizeof(answer), "%s", (new_smsc[0]) ? new_smsc : DEVICE.smsc);
    while (*answer == '+')
      strcpyo(answer, answer + 1);
    sprintf(command, "AT+CSCA=\"+%s\"\r", answer);

    retries=0;
    do
    {
      retries++;
      put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);
      if (strstr(answer, "ERROR"))
        if (retries < 2)
          if (t_sleep(errorsleeptime))
            return 7;
    }
    while (retries < 2 && !strstr(answer,"OK"));
    if (strstr(answer,"ERROR"))
    {
      // 3.1: more detailed error message:
      p = get_gsm_error(answer);
      writelogfile0(LOG_ERR, 1, tb_sprintf("Error: Modem did not accept SMSC%s%s", (*p)? ", " : "", p));
      alarm_handler0(LOG_ERR, tb);
      return 6;
    }
  }

  // -----------------------------------------------------------------------------------------------
  // 3.1beta7, 3.0.9: International Mobile Subscriber Identity is asked once.
  if (DEVICE.identity[0] == 0)
  {
    //writelogfile(LOG_INFO,m odemname,"Querying IMSI");
    sprintf(command,"AT+CIMI\r");
    put_command(command, DEVICE.identity, SIZE_IDENTITY, 1, EXPECT_OK_ERROR);

    // 3.1.5: do not remove ERROR text:
    if (!strstr(DEVICE.identity, "ERROR"))
      while (DEVICE.identity[0] && !isdigitc(DEVICE.identity[0]))
        strcpyo(DEVICE.identity, DEVICE.identity +1);

    // 3.1beta7: If CIMI is not supported, try CGSN (Product Serial Number)
    // TODO: is IMSI title still good?
    if (strstr(DEVICE.identity, "ERROR"))
    {
      sprintf(command,"AT+CGSN\r");
      put_command(command, DEVICE.identity, SIZE_IDENTITY, 1, EXPECT_OK_ERROR);

      // 3.1:
      while (DEVICE.identity[0] && !isdigitc(DEVICE.identity[0]))
        strcpyo(DEVICE.identity, DEVICE.identity +1);
    }
    else
    {
      // 3.1.5: IMSI worked. Log CGSN for informative purposes:
      sprintf(command,"AT+CGSN\r");
      put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);

      if (!strstr(answer, "ERROR"))
        while (answer[0] && !isdigitc(answer[0]))
          strcpyo(answer, answer +1);

      if ((p = strstr(answer, "OK")))
        *p = 0;
      cut_ctrl(answer);
      cutspaces(answer);
      writelogfile(LOG_NOTICE, 0, "CGSN: %s", answer);
    }

    if (!strstr(DEVICE.identity, "ERROR"))
    {
      if ((p = strstr(DEVICE.identity, "OK")))
        *p = 0;
      cut_ctrl(DEVICE.identity);
      cutspaces(DEVICE.identity);
      writelogfile(LOG_NOTICE, 0, "IMSI: %s", DEVICE.identity);
    }
    else
      writelogfile(LOG_NOTICE, 1, "IMSI/CGSN not supported");
  }

  // -----------------------------------------------------------------------------------------------
  // 3.1.5: Check once if reading of messages is not supported:
  // 3.1.7: Do not check if not reading incoming messages:
  if (DEVICE.incoming && !reading_checked)
  {
    reading_checked = 1;
    writelogfile(LOG_INFO, 0, "Checking if reading of messages is supported");

    sprintf(command,"AT+CPMS?\r");
    put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);

    if (strstr(answer, "+CPMS: ,,,,,,,,"))
    {
      sprintf(command,"AT+CPMS=?\r");
      put_command(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR);

      if (strstr(answer, "+CPMS: (),(),()"))
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Error: Looks like your device does not support reading of messages"));
        alarm_handler0(LOG_ERR, tb);
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // 3.1.7: Report details of device once:
  if (DEVICE.report_device_details)
  {
    int save_log_single_lines = log_single_lines;
    int i;
    char tmp[256];
    char *commands[] = {
      "AT+CGMI", "Manufacturer identification",
      "AT+CGMM", "Model identification",
      "AT+CGMR", "Revision identification",
      "AT+CNMI=?", "New message indications, list of supported modes",
      "AT+CNMI?", "New message indications, current settings",
      "AT+CPMS=?", "Preferred message storage, list of supported mem's",
      //"AT+CPMS?", "Preferred message storage, current mem's and counters",
      "AT+CPBS=?", "Phonebook storage, available mem's",
      //"AT+CPBS?", "Phonebook storage, current storage and counters",
      "AT+CMGL=?", "List messages, list of supported stat's",
      "AT+CMGD=?", "Delete message, list of supported values",
      "AT+CPAS=?", "Phone activity status, list of supported stat's",
      "AT+CSCS=?", "TE character set, list of supported charset's",
      "AT+CSCS?", "TE character set, current setting",
      "" // end marker
    };

    DEVICE.report_device_details = 0;
    log_single_lines = 0;
    change_loglevel(LOG_DEBUG);

    writelogfile(LOG_DEBUG, 0, "## Start of device details");

    for (i = 0; commands[i][0]; i += 2)
    {
      if (terminate)
        break;

      snprintf(tmp, sizeof(tmp), "# %s:", commands[i + 1]);
      writelogfile(LOG_DEBUG, 0, tmp);
      sprintf(command, "%s\r", commands[i]);
      put_command0(command, answer, sizeof(answer), 1, EXPECT_OK_ERROR, 1);
    }

    writelogfile(LOG_DEBUG, 0, "## End of device details");

    log_single_lines = save_log_single_lines;
    restore_loglevel();
  }

  // -----------------------------------------------------------------------------------------------
  // TODO: Check if AT+CMGD=? is supported.

  return 0;
}

int initialize_modem_sending(char *new_smsc)
{
  return initmodem(new_smsc, 0);
}

int initialize_modem_receiving()
{
  return initmodem("", 1);
}

#ifndef DISABLE_INET_SOCKET

/* Start Changes by Hubert Gilch, SEP Logistik AG
 *
 * 2 functions for connecting to a socket instead of a serial device
 * in order to use ethernet GPRS-modems
 *
 * Code was "stolen" from interceptty by Scott W. Gifford
 *
 */

struct sockaddr_in inet_resolve(const char *sockname)
{
  struct sockaddr_in sa;
  char *hostname, *netport;
  struct hostent *he;

  if (!(hostname = strdup(sockname)))
    writelogfile(LOG_CRIT, 0, "Couldn't dup string: %s", strerror(errno));

  netport = strchr(hostname, ':');
  *netport = '\0';
  netport++;

  sa.sin_family = AF_INET;

  if (!(he = gethostbyname(hostname)))
    writelogfile(LOG_ERR, 0, "Couldn't resolve name '%s': %s.", hostname,
      (h_errno == HOST_NOT_FOUND) ? "Host not found" :
      ((h_errno == NO_ADDRESS) || (h_errno == NO_DATA)) ? "No data available" :
      (h_errno == NO_RECOVERY) ? "A non-recoverable name server error occured" : (h_errno == TRY_AGAIN) ? "A temporary error occured." : "An unknown error occured");

  memcpy(&(sa.sin_addr), he->h_addr, he->h_length);

#if 0
  if (!(se = getservbyname(netport)))
    writelogfile(LOG_ERR, 0, "Couldn't resolve port.");

  host_port = htons(se->s_port);
#endif

  if (!(sa.sin_port = htons(atoi(netport))))
    writelogfile(LOG_ERR, 0, "Couldn't figure out port number.");

  free(hostname);

  return sa;
}

int open_inet_socket(char *backend)
{
  struct sockaddr_in sa;
  int fd;
  int socketflags;
  int retries = 0;

  sa = inet_resolve(backend + 1);	// cut first character @
  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 3)
  {
    tb_sprintf("Couldn't open socket: %s: %s", backend, strerror(errno));
    writelogfile0(LOG_ERR, 0, tb);
    alarm_handler0(LOG_ERR, tb);
    return -1;
  }

  while (connect(fd, (struct sockaddr *) &sa, sizeof(sa)) != 0)
  {
    retries++;

    if (terminate || (DEVICE.socket_connection_retries != -1 && retries > DEVICE.socket_connection_retries))
    {
      close(fd);
      return (terminate)? -2 : -1;
    }

    tb_sprintf("Couldn't connect socket %s, error: %s, waiting %i sec.", backend, strerror(errno), DEVICE.socket_connection_errorsleeptime);

    if (retries - 1 == DEVICE.socket_connection_alarm_after)
    {
      writelogfile0(LOG_ERR, 0, tb);
      alarm_handler0(LOG_ERR, tb);
    }
    else
    {
      // Do not log the first failure:
      if (retries > 1)
        writelogfile(LOG_INFO, 0, tb);
    }

    t_sleep(DEVICE.socket_connection_errorsleeptime);
  }

  socketflags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, socketflags | O_NONBLOCK);

  return fd;
}

/*
 * End Changes by Hubert Gilch
 */

#endif

int openmodem()
{
  int retries = 0;

  // 3.1.7:
  //modem_handle = open(DEVICE.device, O_RDWR | O_NOCTTY | O_NONBLOCK);

  /*
   * if devicename starts with "@" it is not a serial device but
   * a socket, so open a socket instead a device file
   *
   * Change by Hubert Gilch, SEP Logistik AG
   */
#ifndef DISABLE_INET_SOCKET
  if (DEVICE_IS_SOCKET)
    modem_handle = open_inet_socket(DEVICE.device);
  else
#endif
  {
    // 3.1.12:
    if (DEVICE.modem_disabled)
    {
      struct stat statbuf;

      if (stat(DEVICE.device, &statbuf) != 0)
      {
        FILE *fp;

        if ((fp = fopen(DEVICE.device, "w")))
          fclose(fp);
      }
    }

    // 3.1.7:
    while ((modem_handle = open(DEVICE.device, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
    {
      retries++;

      if (terminate || (DEVICE.device_open_retries != -1 && retries > DEVICE.device_open_retries))
        break;

      tb_sprintf("Couldn't open serial port %s, error: %s, waiting %i sec.", DEVICE.device, strerror(errno), DEVICE.device_open_errorsleeptime);

      if (retries - 1 == DEVICE.device_open_alarm_after)
      {
        writelogfile0(LOG_ERR, 0, tb);
        alarm_handler0(LOG_ERR, tb);
      }
      else
        writelogfile(LOG_INFO, 0, tb);

      t_sleep(DEVICE.device_open_errorsleeptime);
    }
  }

  if (modem_handle < 0)
  {
    if (modem_handle == -1)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf((DEVICE_IS_SOCKET)? "Cannot open socket %s, error: %s" : "Cannot open serial port %s, error: %s", DEVICE.device, strerror(errno)));
      alarm_handler0(LOG_ERR, tb);
    }
    return -1;
  }

  if (strstr(smsd_version, "beta"))
  {
    if (DEVICE_IS_SOCKET)
      writelogfile(LOG_INFO, 0, "Socket %s opened as %i", DEVICE.device, modem_handle);
    else
      writelogfile(LOG_INFO, 0, "Serial port %s opened as %i, rtscts: %i, baudrate: %i", DEVICE.device, modem_handle, DEVICE.rtscts, DEVICE.baudrate);
  }

  return modem_handle;
}

int talk_with_modem()
{
  int result = 0;
  int n;
  char tmp[256];
  struct termios newtset, oldtset;
  char newdevice[PATH_MAX];
  int stdinflags;
  int set_nonblock = 0;
  int idle;

  modem_handle = -1;

  stdinflags = fcntl(STDIN_FILENO, F_GETFL); 

  if (!(stdinflags & O_NONBLOCK)) 
  {
    if (fcntl(STDIN_FILENO, F_SETFL, stdinflags | O_NONBLOCK) == -1) 
      printf("Failed to set STDIN nonblock.\n");
    else
      set_nonblock = 1;
  }

  tcgetattr(STDIN_FILENO, &oldtset);
  newtset = oldtset;
  newtset.c_lflag &= ~(ECHO | ICANON);
  newtset.c_iflag &= ~ICRNL;
  newtset.c_cc[VMIN] = 1;
  newtset.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtset);

  printf("Communicating with %s. ( Press Ctrl-C to abort. )\n", process_title);
  printf("( If you need to send Ctrl-Z, change the suspend character first, like stty susp \\^N )\n");
  writelogfile(LOG_CRIT, 0, "Communicating with terminal.");

  printf("Default device is %s\n", DEVICE.device);
  printf("Press Enter to start or type an another device name.\n");
  *newdevice = 0;

  while (!terminate)
  {
    idle = 0;

    if ((n = read(STDIN_FILENO, tmp, (modem_handle != -1)? sizeof(tmp) -1 : 1)) > 0)
    {
      if (modem_handle != -1)
      {
        tmp[n] = 0;
        write_to_modem(tmp, 5, 0, 1);
      }
      else
      {
        if (*tmp == 13)
        {
          printf("\n");
          fflush(stdout);
          if (*newdevice)
            strcpy(DEVICE.device, newdevice);
          printf("Opening device %s\n", DEVICE.device);
          if (openmodem() == -1)
          {
            printf("Cannot open device %s, cause: %s.\n", DEVICE.device, strerror(errno));
            *newdevice = 0;
            continue;
          }
          setmodemparams();
          printf("Ready.\n");
          result = 1;
        }
        else if (*tmp)
        {
          printf("%c", *tmp);
          fflush(stdout);
          if (*tmp == 127 || *tmp == 8)
          {
            if (*newdevice)
              newdevice[strlen(newdevice) -1] = 0;
          }
          else
          {
            if (strlen(newdevice) < sizeof(newdevice) -1)
              sprintf(strchr(newdevice, 0), "%c", *tmp);
            else
            {
              printf("\nDevice name too long.\n");
              *newdevice = 0;
              continue;
            }
          }
        }
      }
    }
    else
      idle = 1;

    if (modem_handle != -1)
    {
      if ((n = read(modem_handle, tmp, sizeof(tmp) -1)) > 0)
      {
        // 3.1.12:
        if (log_read_from_modem)
        {
          char temp[SIZE_LOG_LINE];
          int i;
          char *answer = tmp;

          snprintf(temp, sizeof(temp), "read_from_modem: got=%i:", n);
          for (i = 0; i < n; i++)
          {
            if (strlen(temp) >= sizeof(temp) - 6)
            {
              strcpy(temp, "ERROR: too much data");
              break;
            }

            sprintf(strchr(temp, 0), " %02X[%c]", (unsigned char) answer[i], ((unsigned char) answer[i] >= ' ') ? answer[i] : '.');
          }

          writelogfile(LOG_CRIT, 0, temp);
        }

        // 3.1.12:
        if (DEVICE_IS_SOCKET)
          negotiate_with_telnet(tmp, &n);

        write(STDOUT_FILENO, tmp, n);
        idle = 0;
      }
    }

    if (idle)
      usleep_until(time_usec() + 100000);
  }

  if (modem_handle >= 0)
    close(modem_handle);

  if (set_nonblock)
    fcntl(STDIN_FILENO, F_SETFL, stdinflags & ~O_NONBLOCK); 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldtset);

  return result;
}

// Return value:
// -2 = terminated
// -1 = modem is not registered
// >= 0 = number of retries, modem is registered
int wait_network_registration(int waitnetwork_errorsleeptime, int retry_count)
{
  char answer[500];
  int success = 0;
  int retries = 0;
  int registration_denied = 0;
  static int registration_ok = 0;
  char *p;
  // 3.1.14:
  static char prev_lac[32] = "";
  static char prev_ci[32] = "";
  char lac[32];
  char ci[32];

  writelogfile(LOG_INFO, 0, "Checking if Modem is registered to the network");

  do
  {
    flush_smart_logging();

    // 3.1: signal quality is logged:
    if (retries > 0)
    {
      put_command("AT+CSQ\r", answer, sizeof(answer), 2, EXPECT_OK_ERROR);

      // 3.1.5: ...with details:
      explain_csq(LOG_NOTICE, 0, answer, DEVICE.signal_quality_ber_ignore);
    }

    put_command("AT+CREG?\r", answer, sizeof(answer), 2, "(\\+CREG:.*OK)|(ERROR)");

    // 3.1.14:
    if (get_loglevel() >= DEVICE.loglevel_lac_ci)
    {
      if ((p = strchr(answer, '\r')))
        *p = ',';
      getfield(answer, 3, lac, sizeof(lac));
      getfield(answer, 4, ci, sizeof(ci));
      if (strlen(ci) > 4)
        memmove(ci, ci +strlen(ci) -4, 5);
    }

    // 3.1.1: Some modem include spaces in the response:
    while ((p = strchr(answer, ' ')))
      strcpyo(p, p +1);

    // 3.1.1: Drop additional fields:
    if ((p = strchr(answer, ',')))
      if ((p = strchr(p +1, ',')))
        *p = 0;

    // 3.1.1: Some modem (Motorola) gives values using three digits like "000,001":
    if ((p = strstr(answer, ",00")))
      strcpyo(p +1, p +3);

    // 3.1:
    // Second field is tested.
    if (strstr(answer, ",1"))
    {
      writelogfile(LOG_INFO, 0, "Modem is registered to the network");
      success = 1;
    }
    else if (strstr(answer, ",5"))
    {
      writelogfile(LOG_INFO, 0, "Modem is registered to a roaming partner network");
      success = 1;
    }
    // 3.1.1: 3 - Registration denied is handled
    else if (strstr(answer, ",3"))
    {
      // 3.1.5: After a SIM has once been successfully registered to the network, failure with registration
      // does not stop the modem process.
      //if (registration_denied < 2)
      if (registration_ok || registration_denied < 2)
      {
        writelogfile(LOG_INFO, 1, "Modem said: registration denied. Retrying.");
        registration_denied++;
        if (t_sleep(waitnetwork_errorsleeptime))
          return -2;
      }
      else
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Error: registration is denied."));
        alarm_handler0(LOG_ERR, tb);
        abnormal_termination(0);
      }
    }
    else if (strstr(answer,"ERROR"))
    {
      writelogfile(LOG_INFO, 1, "Ignoring that modem does not support +CREG command.");
      success = 1;
      DEVICE.check_network = 0;
    }
    else if (strstr(answer,"+CREG:"))
    {
      // 3.1.14: Skip logging if defined. Call alarmhandler.
      if (retries >= DEVICE.log_not_registered_after)
      {
        writelogfile0(LOG_NOTICE, 1, tb_sprintf("MODEM IS NOT REGISTERED, WAITING %i SEC. BEFORE RETRYING %i. TIME", waitnetwork_errorsleeptime, retries +1));
        alarm_handler0(LOG_NOTICE, tb);
      }

      if (t_sleep(waitnetwork_errorsleeptime))
        return -2;
    }
    else
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("Error: Unexpected answer from Modem after +CREG?, waiting %i sec. before retrying", waitnetwork_errorsleeptime));
      alarm_handler0(LOG_ERR, tb);
      if (t_sleep(waitnetwork_errorsleeptime))
        return -2;
    }

    if (!success)
      retries++;
  }
  while (!success && retries < retry_count);

  if (!success)
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Error: Modem is not registered to the network"));
    alarm_handler0(LOG_ERR, tb);
    return -1;
  }

  // 3.1.14:
  if (get_loglevel() >= DEVICE.loglevel_lac_ci && *lac && *ci)
  {
    if (*prev_lac && *prev_ci)
    {
      if (strcmp(prev_lac, lac))
        writelogfile(DEVICE.loglevel_lac_ci, 0, "Location area code changed: %s -> %s", prev_lac, lac);

      if (strcmp(prev_ci, ci))
        writelogfile(DEVICE.loglevel_lac_ci, 0, "Cell ID changed: %s -> %s", prev_ci, ci);

      if (strcmp(prev_lac, lac) || strcmp(prev_ci, ci))
      {
        put_command("AT+CSQ\r", answer, sizeof(answer), 2, EXPECT_OK_ERROR);
        explain_csq(DEVICE.loglevel_lac_ci, 0, answer, DEVICE.signal_quality_ber_ignore);
      }
    }
    else
    {
      writelogfile(DEVICE.loglevel_lac_ci, 0, "Location area code: %s, Cell ID: %s", lac, ci);
      put_command("AT+CSQ\r", answer, sizeof(answer), 2, EXPECT_OK_ERROR);
      explain_csq(DEVICE.loglevel_lac_ci, 0, answer, DEVICE.signal_quality_ber_ignore);
    }

    snprintf(prev_lac, sizeof(prev_lac), "%s", lac);
    snprintf(prev_ci, sizeof(prev_ci), "%s", ci);
  }

  registration_ok = 1;

  return retries;
}

int try_closemodem(int force)
{
  int keep_open;

  if (force)
    keep_open = 0;
  else
    keep_open = DEVICE.keep_open;

  if (modem_handle >= 0 && !keep_open)
  {
    if (1 && strstr(smsd_version, "beta"))
    {
      writelogfile(LOG_INFO, 0, "Device %s (%i) closed", DEVICE.device, modem_handle);
      writelogfile(LOG_INFO, 0, "***********");
    }
#ifdef DEBUGMSG
  printf("!! Closing device %s\n", DEVICE.device);
#endif 
    close(modem_handle);
    modem_handle = -2;
  }

  return (modem_handle >= 0);
}

int try_openmodem()
{
  int result = 1;

  if (modem_handle >= 0)
  {
#ifdef DEBUGMSG
  printf("!! Opening device %s: already open\n", DEVICE.device);
#endif 
    return 1;
  }

#ifdef DEBUGMSG
  printf("!! Opening device %s\n", DEVICE.device);
#endif 
  if (openmodem() == -1)
  {
    result = 0;
#ifdef DEBUGMSG
  printf("!! Opening FAILED\n");
#endif   
  }
  else  
  {
#ifdef DEBUGMSG
  printf("!! Setting modem parameters\n");
#endif   
    put_command_timeouts = 0;
    setmodemparams();
  }

  return result;
}
