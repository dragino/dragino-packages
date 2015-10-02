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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <grp.h>
#include "extras.h"
#include "cfgfile.h"
#include "smsd_cfg.h"
#include "stats.h"
#include "version.h"
#include "blacklist.h"
#include "whitelist.h"
#include "alarm.h"
#include "logging.h"

int conf_ask = 0;
char *yesno_error = "Invalid %s value: %s\n";

#define strcpy2(dest, value) copyvalue(dest, sizeof(dest) -1, value, name)

char *tb_sprintf(char* format, ...)
{
  va_list argp;

  va_start(argp, format);
  vsnprintf(tb, sizeof(tb), format, argp);
  va_end(argp);

  return tb;
}

void startuperror(char* format, ...)
{
  va_list argp;
  char text[2048];

  va_start(argp, format);
  vsnprintf(text, sizeof(text), format, argp);
  va_end(argp);

  // Perhaps more safe way than passing a null pointer to realloc:
  if (!startup_err_str)
  {
    startup_err_str = (char *)malloc(strlen(text) +1);
    if (startup_err_str)
      *startup_err_str = 0;
  }
  else
    startup_err_str = (char *)realloc((void *)startup_err_str, strlen(startup_err_str) + strlen(text) +1);

  if (startup_err_str)
  {
    strcat(startup_err_str, text);
    startup_err_count++;
  }
}

// Helper function for copying to prevent buffer overflows:
char *copyvalue(char *dest, size_t maxlen, char *value, char *keyword)
{
  if (strlen(value) > maxlen)
  {
    if (keyword)
      startuperror("Too long value for \"%s\" (%s)\n", keyword, value);
    else
      startuperror("Too long value (%s)\n", value);
    return NULL;
  }

  // 3.1.10: allow overlapped buffers:
  //snprintf(dest, maxlen -1, "%s", value);
  memmove(dest, value, strlen(value) +1);

  return dest;
}

int set_level(char *section, char *name, char *value)
{
  int result = 0;
  char *p;
  int i;

  if ((p = malloc(strlen(value) +1)))
  {
    strcpy(p, value);
    for (i = 0; p[i]; i++)
      p[i] = toupper((int)p[i]);

    if ((result = atoi(value)) < 1)
    {
      if (strstr(p, "DEBUG"))
        result = 7;
      else if (strstr(p, "INFO"))
        result = 6;
      else if (strstr(p, "NOTICE"))
        result = 5;
      else if (strstr(p, "WARNING"))
        result = 4;
      else if (strstr(p, "ERROR"))
        result = 3;
      else if (strstr(p, "CRITICAL"))
        result = 2;
    }

    free(p);
  }

  if (result < 1)
    startuperror("Invalid value for %s %s: %s\n", section, name, value);

  return result;
}

void initcfg()
{
  int i;
  int j;

  autosplit=3;
  receive_before_send=0;
  store_received_pdu=1;
  store_sent_pdu = 1;
  validity_period=255;
  delaytime=10;
  delaytime_mainprocess = -1;
  blocktime=60*60;
  blockafter = 3;
  errorsleeptime=10;
  blacklist[0]=0;
  whitelist[0]=0;
  eventhandler[0]=0;
  checkhandler[0]=0;
  alarmhandler[0]=0;
  logfile[0]=0;
  loglevel=-1;  // Will be changed after reading the cfg file if stil -1
  log_unmodified = 0;
  alarmlevel=LOG_WARNING;

  strcpy(d_spool,"/var/spool/sms/outgoing");
  strcpy(d_incoming,"/var/spool/sms/incoming");
  *d_report = 0;
  *d_phonecalls = 0;
  *d_saved = 0;
  strcpy(d_checked,"/var/spool/sms/checked");
  d_failed[0]=0;
  d_sent[0]=0;
  d_stats[0]=0;
  suspend_filename[0]=0;
  stats_interval=60*60;
  status_interval=1;

// 3.1.5: If shared memory is not in use, stats is not useable (all zero):
#ifndef NOSTATS
  stats_no_zeroes=0;
#else
  stats_no_zeroes=1;
#endif

  decode_unicode_text=0;
  internal_combine = 1;
  internal_combine_binary = -1;
  keep_filename = 1;
  store_original_filename = 1;
  date_filename = 0;
  regular_run[0] = 0;
  regular_run_interval = 5 * 60;
  admin_to[0] = 0;
  filename_preview = 0;
  incoming_utf8 = 0;
  outgoing_utf8 = 1;
  log_charconv = 0;
  log_read_from_modem = 0;
  log_single_lines = 1;
  executable_check = 1;
  keep_messages = 0;
  *priviledged_numbers = 0;
  ic_purge_hours = 24;
  ic_purge_minutes = 0;
  ic_purge_read = 1;
  ic_purge_interval = 30;
  strcpy(shell, "/bin/sh");
  *adminmessage_device = 0;
  smart_logging = 0;
  status_signal_quality = 1;
  status_include_counters = 1;
  hangup_incoming_call = 0;
  max_continuous_sending = 5 *60;
  voicecall_hangup_ath = 0;

  trust_outgoing = 0;
  ignore_outgoing_priority = 0;
  spool_directory_order = 0;

  trim_text = 1;

  message_count = 0;

  username[0] = 0;
  groupname[0] = 0;

  strcpy(infofile, "/var/run/smsd.working");
  strcpy(pidfile, "/var/run/smsd.pid");

  terminal = 0;
  os_cygwin = 0;

  *international_prefixes = 0;
  *national_prefixes = 0;

  for (i = 0; i < NUMBER_OF_MODEMS; i++)
  {
    queues[i].name[0]=0;
    queues[i].directory[0]=0;
    for (j=0; j<NUMS; j++)
      queues[i].numbers[j][0]=0;
  }
  for (i = 0; i < NUMBER_OF_MODEMS; i++)
  {
    devices[i].name[0]=0;
    devices[i].number[0]=0;
    devices[i].device[0]=0;
    devices[i].device_open_retries = 1;
    devices[i].device_open_errorsleeptime = 30;
    devices[i].device_open_alarm_after = 0;
    devices[i].identity[0] = 0;
    devices[i].conf_identity[0] = 0;
    //devices[i].identity_header[0] = 0;
    devices[i].incoming=0;
    devices[i].outgoing=1;
    devices[i].pin[0]=0;
    devices[i].pinsleeptime=0;
    devices[i].smsc[0]=0;
    devices[i].baudrate=115200;
    devices[i].send_delay=0;
    devices[i].send_handshake_select=1;
    devices[i].cs_convert=1;
    devices[i].initstring[0]=0;
    devices[i].initstring2[0]=0;
    devices[i].eventhandler[0]=0;
    devices[i].eventhandler_ussd[0]=0;
    devices[i].ussd_convert=0;
    devices[i].report=0;
    devices[i].phonecalls=0;
    devices[i].phonecalls_purge[0]=0;
    devices[i].phonecalls_error_max=3;
    devices[i].rtscts=1;
    devices[i].read_memory_start=1;
    strcpy(devices[i].mode,"new");

    for (j = 0; j < NUMBER_OF_MODEMS; j++)
      devices[i].queues[j][0]=0;

    devices[i].primary_memory[0]=0;
    devices[i].secondary_memory[0]=0;
    devices[i].secondary_memory_max=-1;
    devices[i].pdu_from_file[0]=0;
    devices[i].sending_disabled=0;
    devices[i].modem_disabled=0;
    devices[i].decode_unicode_text=-1;
    devices[i].internal_combine=-1;
    devices[i].internal_combine_binary=-1;
    devices[i].pre_init = 1;
    devices[i].check_network=1;
    devices[i].admin_to[0]=0;
    devices[i].message_limit=0;
    devices[i].message_count_clear=0;
    devices[i].keep_open = 1; // 0;
    devices[i].dev_rr[0] = 0;
    devices[i].dev_rr_post_run[0] = 0;
    devices[i].dev_rr_interval = 5 * 60;
    devices[i].dev_rr_cmdfile[0] = 0;
    devices[i].dev_rr_cmd[0] = 0;
    devices[i].dev_rr_logfile[0] = 0;
    devices[i].dev_rr_loglevel = LOG_NOTICE;
    devices[i].dev_rr_statfile[0] = 0;
    devices[i].logfile[0] = 0;
    devices[i].loglevel = -1;
    devices[i].messageids = 2;
    devices[i].voicecall_vts_list = 0;
    devices[i].check_memory_method = CM_CPMS;
    strcpy(devices[i].cmgl_value, "4");
    devices[i].priviledged_numbers[0] = 0;
    devices[i].read_timeout = 5;
    devices[i].ms_purge_hours = 6;
    devices[i].ms_purge_minutes = 0;
    devices[i].ms_purge_read = 1;
    devices[i].detect_message_routing = 1;
    devices[i].detect_unexpected_input = 1;
    devices[i].unexpected_input_is_trouble = 1;
    devices[i].adminmessage_limit=0;
    devices[i].adminmessage_count_clear=0;
    devices[i].voicecall_ignore_modem_response = 0;
    devices[i].voicecall_hangup_ath = -1;
    devices[i].voicecall_vts_quotation_marks = 0;
    devices[i].voicecall_cpas = 0;
    devices[i].voicecall_clcc = 0;
    devices[i].status_signal_quality = -1;
    devices[i].status_include_counters = -1;
    devices[i].communication_delay = 0;
    devices[i].hangup_incoming_call = -1;
    devices[i].max_continuous_sending = -1;
    devices[i].startstring[0] = 0;
    devices[i].startsleeptime = 3;
    devices[i].stopstring[0] = 0;
    devices[i].socket_connection_retries = 11;
    devices[i].socket_connection_alarm_after = 0;
    devices[i].socket_connection_errorsleeptime = 5;
    devices[i].report_device_details = (strstr(smsd_version, "beta"))? 1 : 0;
    devices[i].using_routed_status_report = 0;
    devices[i].routed_status_report_cnma = 1;
    devices[i].needs_wakeup_at = 0;
    devices[i].keep_messages = 0;
    devices[i].trust_spool = 1;
    devices[i].smsc_pdu = 0;
    devices[i].telnet_login[0] = 0;
    snprintf(devices[i].telnet_login_prompt, sizeof(devices[i].telnet_login_prompt), "%s", TELNET_LOGIN_PROMPT_DEFAULT);
    snprintf(devices[i].telnet_login_prompt_ignore, sizeof(devices[i].telnet_login_prompt_ignore), "%s", TELNET_LOGIN_PROMPT_IGNORE_DEFAULT);
    devices[i].telnet_password[0] = 0;
    snprintf(devices[i].telnet_password_prompt, sizeof(devices[i].telnet_password_prompt), "%s", TELNET_PASSWORD_PROMPT_DEFAULT);
    devices[i].signal_quality_ber_ignore = 0;
    devices[i].verify_pdu = 0;
    devices[i].loglevel_lac_ci = 6;
    devices[i].log_not_registered_after = 0;
  }
  startup_err_str = NULL;
  startup_err_count = 0;

  *language_file = 0;
  translate_incoming = 1;
  *yes_chars = 0;
  *no_chars = 0;
  strcpy(yes_word, "yes");
  strcpy(no_word, "no");

  snprintf(datetime_format, sizeof(datetime_format), "%s", DATETIME_DEFAULT);

  // 3.1.14:
  //snprintf(logtime_format, sizeof(logtime_format), "%s", LOGTIME_DEFAULT);
  *logtime_format = 0;

  snprintf(date_filename_format, sizeof(date_filename_format), "%s", DATE_FILENAME_DEFAULT);

  enable_smsd_debug = 0;
  ignore_exec_output = 0;
  conf_umask = 0;

#ifdef USE_LINUX_PS_TRICK
  use_linux_ps_trick = 1;
#else
  use_linux_ps_trick = 0;
#endif

  // 3.1.14:
  logtime_us = 0;
  logtime_ms = 0;

  // 3.1.14:
  shell_test = 1;
}

char *ask_value(char *section, char *name, char *value)
{
  int m;
  char tmp[4096];
  int i;
  int n;

  if (*value == '?')
  {
    while (*value && strchr("? \t", *value))
      strcpyo(value, value +1);

    if (!conf_ask)
    {
      getsubparam_delim(value, 1, tmp, sizeof(tmp), '|');
      strcpy(value, tmp);
    }
    else
    {
      conf_ask++;
      while (1)
      {
        printf("Value for \"%s %s\" (Enter for the first value, 0 to exit):\n",
               (section && *section)? section : "global", name);
        m = 0;
        while (getsubparam_delim(value, m +1, tmp, sizeof(tmp), '|'))
          printf("%i) %s\n", ++m, tmp);
        printf("%i) Other\n", ++m);
        printf("Select 1 to %i: ", m);
        fflush(stdout);
        if ((n = read(STDIN_FILENO, tmp, sizeof(tmp) -1)) > 0)
          tmp[n] = 0;
        else
          *tmp = 0;
        cut_ctrl(tmp);
        cutspaces(tmp);
        if (!(*tmp))
          strcpy(tmp, "1");
        if (strcmp("0", tmp) == 0)
        {
          printf("Exiting...\n");
          fflush(stdout);
          exit(0);
        }
        i = atoi(tmp);
        if (i < 1 || i > m)
        {
          printf("Invalid selection.\n");
          fflush(stdout);
          sleep(1);
          continue;
        }        

        if (i != m)
          getsubparam_delim(value, i, tmp, sizeof(tmp), '|');
        else
        {
          printf("Enter value: ");
          fflush(stdout);
          if ((n = read(STDIN_FILENO, tmp, sizeof(tmp) -1)) > 0)
            tmp[n] = 0;
          else
            *tmp = 0;
          cut_ctrl(tmp);
          cutspaces(tmp);
          if (!(*tmp))
          {
            printf("Empty value is not enough.\n");
            fflush(stdout);
            sleep(1);
            continue;
          }
        }
        strcpy(value, tmp);
        break;
      }
    }
  }

  return value;
}

int readcfg()
{
  FILE* File;
  char devices_list[4096];
  char name[64];
  char value[4096];
  char tmp[4096];
  char device_name[32];
  int result;
  int j, q;
  char *p;
  int newdevice;
  int read_default;
  #define NEWDEVICE devices[newdevice]

  // 3.1.7: no need to change devices list in smsd.conf when communicating:
  // *devices_list = 0;
  strcpy(devices_list, communicate);
  *device_name = 0;

  File=fopen(configfile,"r");
  if (File)
  {
    /* read global parameter */

    // 3.1beta7: all errors are reported, not just the first one.
    while ((result = my_getline(File, name, sizeof(name), value, sizeof(value))) != 0)
    {
      if (result == -1)
      {
        startuperror("Syntax error: %s\n", value);
        continue;
      }

      if (strcasecmp(name,"devices")==0)
        strcpy2(devices_list, ask_value(0, name, value));
      else
      if (strcasecmp(name,"spool")==0)
        strcpy2(d_spool, ask_value(0, name, value));
      else
      if (strcasecmp(name,"outgoing")==0)
        strcpy2(d_spool, ask_value(0, name, value));
      else
      if (strcasecmp(name,"stats")==0)
        strcpy2(d_stats, ask_value(0, name, value));
      else
      if (strcasecmp(name,"suspend")==0)
        strcpy2(suspend_filename, ask_value(0, name, value));
      else
      if (strcasecmp(name,"failed")==0)
        strcpy2(d_failed, ask_value(0, name, value));
      else
      if (strcasecmp(name,"incoming")==0)
        strcpy2(d_incoming, ask_value(0, name, value));
      else
      if (strcasecmp(name,"report")==0)
        strcpy2(d_report, ask_value(0, name, value));
      else
      if (strcasecmp(name,"phonecalls")==0)
        strcpy2(d_phonecalls, ask_value(0, name, value));
      else
      if (strcasecmp(name,"saved")==0)
        strcpy2(d_saved, ask_value(0, name, value));
      else
      if (strcasecmp(name,"checked")==0)
        strcpy2(d_checked, ask_value(0, name, value));
      else
      if (strcasecmp(name,"sent")==0)
        strcpy2(d_sent, ask_value(0, name, value));
      else 
      if (strcasecmp(name,"mypath")==0)
        // Removed in > 3.0.1 because this is not used. Setting is accepted because of backward compatibility.
        ;
      else
      if (strcasecmp(name,"delaytime")==0)
        delaytime=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"delaytime_mainprocess")==0)
        delaytime_mainprocess=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"blocktime")==0)
        blocktime=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"blockafter")==0)
        blockafter=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"stats_interval")==0)
        stats_interval=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"status_interval")==0)
        status_interval=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"stats_no_zeroes")==0)
      {
        if ((stats_no_zeroes = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"errorsleeptime")==0)
        errorsleeptime=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"eventhandler")==0)
        strcpy2(eventhandler, ask_value(0, name, value));
      else
      if (strcasecmp(name,"checkhandler")==0)
        strcpy2(checkhandler, ask_value(0, name, value));	
      else
      if (strcasecmp(name,"alarmhandler")==0)
        strcpy2(alarmhandler, ask_value(0, name, value));
      else
      if (strcasecmp(name,"blacklist")==0)
        strcpy2(blacklist, ask_value(0, name, value));
      else
      if (strcasecmp(name,"whitelist")==0)
        strcpy2(whitelist, ask_value(0, name, value));
      else
      if (strcasecmp(name,"logfile")==0)
        strcpy2(logfile, ask_value(0, name, value));
      else
      if (strcasecmp(name,"loglevel")==0)
        loglevel = set_level("global", name, ask_value(0, name, value));
      else
      if (strcasecmp(name,"log_unmodified")==0)
      {
        if ((log_unmodified = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"alarmlevel")==0)
        alarmlevel = set_level("global", name, ask_value(0, name, value));
      else
      if (strcasecmp(name,"autosplit")==0)
	autosplit=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"receive_before_send")==0)
      {
        if ((receive_before_send = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"store_received_pdu")==0)
        store_received_pdu=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"store_sent_pdu")==0)
        store_sent_pdu=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"validity")==0)
      {
        if ((validity_period = parse_validity(ask_value(0, name, value), -1)) == -1)
          startuperror("Invalid validity period: %s\n", value);
      }
      else
      if (strcasecmp(name,"decode_unicode_text")==0)
      {
        if ((decode_unicode_text = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"internal_combine")==0)
      {
        if ((internal_combine = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"internal_combine_binary")==0)
      {
        if ((internal_combine_binary = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"keep_filename")==0)
      {
        if ((keep_filename = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"store_original_filename")==0)
      {
        if ((store_original_filename = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"date_filename")==0)
        date_filename = atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"regular_run")==0)
        strcpy2(regular_run, ask_value(0, name, value));
      else
      if (strcasecmp(name,"regular_run_interval")==0)
      {
        if ((regular_run_interval = atoi(ask_value(0, name, value))) <= 0)
          startuperror("Invalid global regular_run_interval: %s\n", value);
      }
      else
      if (strcasecmp(name,"admin_to")==0)
        strcpy2(admin_to, ask_value(0, name, value));
      else
      if (strcasecmp(name,"filename_preview")==0)
        filename_preview=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"incoming_utf8")==0)
      {
        if ((incoming_utf8 = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"outgoing_utf8")==0)
      {
        if ((outgoing_utf8 = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"log_charconv")==0)
      {
        if ((log_charconv = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"log_read_from_modem")==0)
      {
        if ((log_read_from_modem = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"log_single_lines")==0)
      {
        if ((log_single_lines = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"executable_check")==0)
      {
        if ((executable_check = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"keep_messages")==0)
      {
        if ((keep_messages = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"user")==0)
        strcpy2(username, ask_value(0, name, value));
      else
      if (strcasecmp(name,"group")==0)
        strcpy2(groupname, ask_value(0, name, value));
      else
      if (strcasecmp(name,"infofile")==0)
        strcpy2(infofile, ask_value(0, name, value));
      else if (strcasecmp(name,"pidfile")==0)
        strcpy2(pidfile, ask_value(0, name, value));
      else
      if (strcasecmp(name,"terminal")==0)
      {
        if ((terminal = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"os_cygwin")==0)
      {
        if ((os_cygwin = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"language_file")==0)
        strcpy2(language_file, ask_value(0, name, value));
      else
      if (strcasecmp(name,"datetime")==0)
        strcpy2(datetime_format, ask_value(0, name, value));
      else
      if (strcasecmp(name,"datetime_format")==0)
        strcpy2(datetime_format, ask_value(0, name, value));
      else
      if (strcasecmp(name,"logtime_format")==0)
        strcpy2(logtime_format, ask_value(0, name, value));
      else
      if (strcasecmp(name,"date_filename_format")==0)
        strcpy2(date_filename_format, ask_value(0, name, value));
      else
      if (strcasecmp(name,"international_prefixes")==0 ||
          strcasecmp(name,"national_prefixes")==0)
      {
        ask_value(0, name, value);

        p = value;
        while (*p)
        {
          if (is_blank(*p))
            strcpyo(p, p +1);
          else
            p++;
        }

        while ((p = strstr(value, ",,")))
          strcpyo(p, p +1);

        if (strcasecmp(name,"international_prefixes")==0)
          p = international_prefixes;
        else
          p = national_prefixes;

        sprintf(p, "%s%c", value, 0);
        while (*p)
        {
          if (*p == ',')
            *p = 0;
          p++;
        }
      }
      else
      if (strcasecmp(name,"priviledged_numbers")==0)
      {
        ask_value(0, name, value);

        for (j = 1; ; j++)
        {
          if (j >= 25) // It's from A to Y and Z is last in sorting...
          {
            startuperror("Too many global priviledged numbers.\n");
            break;
          }

          if (getsubparam(value, j, tmp, sizeof(tmp)))
          {
            // If not empty, buffer is terminated with double-zero.
            p = priviledged_numbers;
            while (*p)
              p = strchr(p, 0) +1;
            if ((ssize_t)strlen(tmp) <= SIZE_PRIVILEDGED_NUMBERS -2 -(p - priviledged_numbers))
            {
              strcpy(p, tmp);
              *(p +strlen(tmp) +1) = 0;
            }
            else
              startuperror("Not enough space for global priviledged incoming numbers.\n");
          }
          else
            break;
        }
      }
      else
      if (strcasecmp(name,"enable_smsd_debug")==0)
      {
        if ((enable_smsd_debug = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (!strcasecmp(name, "ignore_exec_output"))
      {
        if ((ignore_exec_output = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (!strcasecmp(name, "umask"))
      {
        conf_umask = (mode_t) strtol(ask_value(0, name, value), NULL, 0);
        if (errno == EINVAL)
          startuperror("Invalid value for umask: %s\n", value);
      }
      else
      if (strcasecmp(name,"ic_purge_hours")==0)
        ic_purge_hours=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"ic_purge_minutes")==0)
        ic_purge_minutes=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"ic_purge_read")==0)
      {
        if ((ic_purge_read = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"ic_purge_interval")==0)
        ic_purge_interval=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"shell")==0)
        strcpy2(shell, ask_value(0, name, value));
      else
      if (strcasecmp(name,"adminmessage_device")==0)
        strcpy2(adminmessage_device, ask_value(0, name, value));
      else
      if (strcasecmp(name,"smart_logging")==0)
      {
        if ((smart_logging = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"status_signal_quality")==0)
      {
        if ((status_signal_quality = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"status_include_counters")==0)
      {
        if ((status_include_counters = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"trust_outgoing")==0)
      {
        if ((trust_outgoing = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"ignore_outgoing_priority")==0)
      {
        if ((ignore_outgoing_priority = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"spool_directory_order")==0)
      {
        if ((spool_directory_order = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"trim_text")==0)
      {
        if ((trim_text = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"hangup_incoming_call")==0)
      {
        if ((hangup_incoming_call = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"max_continuous_sending")==0)
        max_continuous_sending=atoi(ask_value(0, name, value));
      else
      if (strcasecmp(name,"voicecall_hangup_ath")==0)
      {
        if ((voicecall_hangup_ath = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"use_linux_ps_trick")==0)
      {
        if ((use_linux_ps_trick = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"logtime_us")==0)
      {
        if ((logtime_us = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"logtime_ms")==0)
      {
        if ((logtime_ms = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
      if (strcasecmp(name,"shell_test")==0)
      {
        if ((shell_test = yesno_check(ask_value(0, name, value))) == -1)
          startuperror(yesno_error, name, value);
      }
      else
        startuperror("Unknown global setting: %s\n", name);
    }

    // 3.1.14:
    if (*logtime_format == 0)
    {
      snprintf(logtime_format, sizeof(logtime_format), "%s", LOGTIME_DEFAULT);

      if (strlen(logtime_format) < sizeof(logtime_format) - 7)
      {
        if (logtime_us)
          strcat(logtime_format, ".timeus");
        else if (logtime_ms)
          strcat(logtime_format, ".timems");
      }
    }

    /* read queue-settings */
    if (gotosection(File, "queues") || gotosection(File, "queue"))
    {
      // 3.1beta3, 3.0.9: inform if there is too many queues defined.
      for (q = 0; ; q++)
      {
        if ((result = my_getline(File, name, sizeof(name), value, sizeof(value))) != 1)
          break;
        if (q >= NUMBER_OF_MODEMS)
        {
          startuperror("Too many queues defined. Increase NUMBER_OF_MODEMS value in src/Makefile.\n");
          break;
        }
        strcpy2(queues[q].name, name);
        strcpy2(queues[q].directory, value);
      }

      if (result==-1)
        startuperror("Syntax error: %s\n",value);
    }

    /* read provider-settings */
    if (gotosection(File, "providers") || gotosection(File, "provider"))
    {
      // TODO: better syntax checking for config file.
      result=my_getline(File,name,sizeof(name),value,sizeof(value));
      while (result==1)
      {
        q = getqueue(name,tmp);
        if (q >= 0)
        {
          // 3.1beta3, 3.0.9: inform if there is too many parameters.
          for (j = 1; ; j++)
          {
            if (getsubparam(value, j, tmp, sizeof(tmp)))
            {
              if (j > NUMS)
              {
                startuperror("Too many parameters for provider %s.\n", name);
                break;
              }
              // 3.1beta4, 3.0.9: remove whitespaces:
              p = tmp;
              while (*p)
              {
                if (is_blank(*p))
                  strcpyo(p, p +1);
                else
                  p++;
              }
#ifdef DEBUGMSG
  printf("!! queues[%i].numbers[%i]=%s\n", q, j-1, tmp);
#endif
	      strcpy2(queues[q].numbers[j-1], tmp);
            }
            else
              break;
          }
        }
        else
          startuperror("Missing queue for %s.\n",name);
        result=my_getline(File,name,sizeof(name),value,sizeof(value));
      }
      if (result==-1)
        startuperror("Syntax error: %s\n",value);
    }

    // 3.1.12:
    if ((p = strchr(devices_list, '*')) && strchr(devices_list, '-'))
    {
      int i, max;

      *p = 0;
      strcpy(tmp, devices_list);
      i = atoi(p +1);
      max = atoi(strstr(p +1, "-") + 1);
      *devices_list = 0;
      while (i <= max)
      {
        sprintf(value, "%s%i,", tmp, i);
        if (sizeof(devices_list) - strlen(devices_list) <= strlen(value))
          break;
        strcpy(strchr(devices_list, 0), value);
        i++;
      }
      if (*devices_list)
        devices_list[strlen(devices_list) - 1] = 0;
    }

    // If devices_list is empty, getsubparam still returns 1 while getting the first name.
    // Now this list is checked with it's own error message:
    if (devices_list[0] == 0)
      startuperror("There are no devices specified.\n");
    else
    {
      // 3.1.5: check if too many devices are specified:
      result = 0;
      while (getsubparam(devices_list, result +1, device_name, sizeof(device_name)))
        result++;

      if (result > NUMBER_OF_MODEMS)
      {
        startuperror("Too many devices specified. Increase NUMBER_OF_MODEMS value in src/Makefile.\n");
        result = 0;
      }
    }

    if (result)
    {
      /* read device-settings */
      for (newdevice = 0; newdevice < NUMBER_OF_MODEMS; newdevice++)
      {
        if (getsubparam(devices_list, newdevice +1, device_name, sizeof(device_name)))
        {
          // 3.1beta7: Check device name, it's also used to create a filename:
          for (j = 0; device_name[j] != 0; j++)
            if (!isalnumc(device_name[j]) && !strchr("_-.", device_name[j]))
              break;

          if (device_name[j] != 0)
	    startuperror("Invalid characters in device name: \n", device_name);
          else if (!strcmp(device_name, "default"))
            startuperror("Device name cannot be \"default\".");
          else if (!strcmp(device_name, "ALL"))
            startuperror("Device name cannot be \"ALL\".");
          //else if (!gotosection(File, device_name))
	  //  startuperror("Could not find device [%s].\n", device_name);
          else
          {
            // 3.1.12: modem section is no more mandatory.
            int device_found = 1;

            for (read_default = 1; read_default >= 0; read_default--)
            {
              if (read_default)
              {
                if (!gotosection(File, "default"))
                  continue;

                strcpy2(NEWDEVICE.name, "default");
              }
              else
              {
                if (!gotosection(File, device_name))
                  device_found = 0;
                strcpy2(NEWDEVICE.name, device_name);
              }

              // 3.1beta7: all errors are reported, not just the first one.
              while (device_found && (result = my_getline(File, name, sizeof(name), value, sizeof(value))) != 0)
              {
                if (result == -1)
                {
                  startuperror("Syntax error: %s\n", value);
                  continue;
                }

  	        if (strcasecmp(name,"number")==0)
	          strcpy2(NEWDEVICE.number, ask_value(NEWDEVICE.name, name, value));
	        else 
	        if (strcasecmp(name,"device")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  // 3.1.12: special devicename:
                  if ((p = strstr(value, "modemname")))
                  {
                    if (strlen(value) -9 +strlen(device_name) < sizeof(tmp))
                    {
                      sprintf(tmp, "%.*s%s%s", (int)(p -value), value, device_name, p +9);
                      strcpy(value, tmp);
                    }
                  }
	          strcpy2(NEWDEVICE.device, value);
                }
	        else
                if (strcasecmp(name,"device_open_retries")==0)
	          NEWDEVICE.device_open_retries=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"device_open_errorsleeptime")==0)
	          NEWDEVICE.device_open_errorsleeptime=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"device_open_alarm_after")==0)
	          NEWDEVICE.device_open_alarm_after=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"queues")==0)
	        {
                  ask_value(NEWDEVICE.name, name, value);

                  // 3.1.5: special queuename:
                  if ((p = strstr(value, "modemname")))
                  {
                    if (strlen(value) -9 +strlen(device_name) < sizeof(tmp))
                    {
                      sprintf(tmp, "%.*s%s%s", (int)(p -value), value, device_name, p +9);
                      strcpy(value, tmp);
                    }
                  }

                  // Inform if there are too many queues defined.
                  for (j = 1; ; j++)
                  {
                    if (getsubparam(value, j, tmp, sizeof(tmp)))
                    {
                      if (j > NUMBER_OF_MODEMS)
                      {
                        startuperror("Too many queues defined for device %s. Increase NUMBER_OF_MODEMS value in src/Makefile.\n", NEWDEVICE.name);
                        break;
                      }
                      strcpy2(NEWDEVICE.queues[j -1], tmp);

                      // Check if given queue is available.
                      if (getqueue(NEWDEVICE.queues[j -1], tmp) < 0)
                        startuperror("Queue %s not found for device %s.\n", NEWDEVICE.queues[j -1], device_name);

                    }
                    else
                      break;
                  }
	        }
	        else
                if (strcasecmp(name,"incoming")==0)
	        {
                  ask_value(NEWDEVICE.name, name, value);
	          if (strcasecmp(value,"high") ==0)
                     NEWDEVICE.incoming=2;
	          else
	          {
	            NEWDEVICE.incoming=atoi(value);
	            if (NEWDEVICE.incoming==0)  // For backward compatibility to older version with boolean value
                    {
	              if ((NEWDEVICE.incoming = yesno_check(value)) == -1)
                        startuperror(yesno_error, name, value);
                    }
	          }
                }
	        else
                if (strcasecmp(name,"outgoing")==0)
                {
	          if ((NEWDEVICE.outgoing = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"cs_convert")==0)
                {
	          if ((NEWDEVICE.cs_convert = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"pin")==0)
	          strcpy2(NEWDEVICE.pin, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"pinsleeptime")==0)
	          NEWDEVICE.pinsleeptime=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"mode")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  if (strcasecmp(value,"ascii")==0)
                    startuperror("Ascii mode is not supported anymore.\n");
                  if ((strcasecmp(value,"old")!=0) && (strcasecmp(value,"new")!=0))
                    startuperror("Invalid mode=%s.\n",value);
                  else
                    strcpy2(NEWDEVICE.mode,value);
                }
	        else
                if (strcasecmp(name,"smsc")==0)
	          strcpy2(NEWDEVICE.smsc, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"baudrate")==0)
	          NEWDEVICE.baudrate=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"send_delay")==0)
                  NEWDEVICE.send_delay=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"send_handshake_select")==0)
                {
	          if ((NEWDEVICE.send_handshake_select = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"memory_start")==0)
                  NEWDEVICE.read_memory_start=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"init")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  copyvalue(NEWDEVICE.initstring, sizeof(NEWDEVICE.initstring) -2, value, name);
                  strcat(NEWDEVICE.initstring,"\r");
                }
                else
                if (strcasecmp(name,"init1")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  copyvalue(NEWDEVICE.initstring, sizeof(NEWDEVICE.initstring) -2, value, name);
                  strcat(NEWDEVICE.initstring,"\r");
                }
                else
                if (strcasecmp(name,"init2")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  copyvalue(NEWDEVICE.initstring2, sizeof(NEWDEVICE.initstring2) -2, value, name);
                  strcat(NEWDEVICE.initstring2,"\r");
                }
	        else
                if (strcasecmp(name,"start")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  copyvalue(NEWDEVICE.startstring, sizeof(NEWDEVICE.startstring) -2, value, name);
                  strcat(NEWDEVICE.startstring,"\r");
                }
                else
                if (strcasecmp(name,"startsleeptime")==0)
	          NEWDEVICE.startsleeptime=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"stop")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  copyvalue(NEWDEVICE.stopstring, sizeof(NEWDEVICE.stopstring) -2, value, name);
                  strcat(NEWDEVICE.stopstring,"\r");
                }
	        else
                if (strcasecmp(name,"eventhandler")==0)
    	          strcpy2(NEWDEVICE.eventhandler, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"eventhandler_ussd")==0)
    	          strcpy2(NEWDEVICE.eventhandler_ussd, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"ussd_convert")==0)
                  NEWDEVICE.ussd_convert=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"report")==0)
                {
    	          if ((NEWDEVICE.report = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"phonecalls")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
	          if (strcasecmp(value,"clip") ==0)
                     NEWDEVICE.phonecalls=2;
	          else
	          {
	            NEWDEVICE.phonecalls=atoi(value);
	            if (NEWDEVICE.phonecalls==0)
                    {
	              if ((NEWDEVICE.phonecalls = yesno_check(value)) == -1)
                        startuperror(yesno_error, name, value);
                    }
	          }
                }
	        else
                if (strcasecmp(name,"phonecalls_purge")==0)
    	          strcpy2(NEWDEVICE.phonecalls_purge, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"phonecalls_error_max")==0)
                  NEWDEVICE.phonecalls_error_max=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"rtscts")==0)
                {
    	          if ((NEWDEVICE.rtscts = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"primary_memory")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  while ((p = strchr(value,'\"')))
                    strcpyo(p, p+1);
  	          strcpy2(NEWDEVICE.primary_memory,value);
                }
                else
                if (strcasecmp(name,"secondary_memory")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  while ((p = strchr(value,'\"')))
                    strcpyo(p, p+1);
  	          strcpy2(NEWDEVICE.secondary_memory,value);
                }
                else
                if (strcasecmp(name,"secondary_memory_max")==0)
                  NEWDEVICE.secondary_memory_max=atoi(ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"pdu_from_file")==0)
    	          strcpy2(NEWDEVICE.pdu_from_file, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"sending_disabled")==0)
                {
    	          if ((NEWDEVICE.sending_disabled = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"modem_disabled")==0)
                {
    	          if ((NEWDEVICE.modem_disabled = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"decode_unicode_text")==0)
                {
    	          if ((NEWDEVICE.decode_unicode_text = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
	        else
                if (strcasecmp(name,"internal_combine")==0)
                {
    	          if ((NEWDEVICE.internal_combine = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else 
                if (strcasecmp(name,"internal_combine_binary")==0)
                {
    	          if ((NEWDEVICE.internal_combine_binary = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else 
                if (strcasecmp(name,"pre_init")==0)
                {
                  if ((NEWDEVICE.pre_init = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"admin_to")==0)
                  strcpy2(NEWDEVICE.admin_to, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"message_limit")==0)
                  NEWDEVICE.message_limit=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"message_count_clear")==0)
                  NEWDEVICE.message_count_clear=atoi(ask_value(NEWDEVICE.name, name, value)) *60;
	        else
                if (strcasecmp(name,"keep_open")==0)
                {
    	          if ((NEWDEVICE.keep_open = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"regular_run")==0)
                  strcpy2(NEWDEVICE.dev_rr, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"regular_run_post_run")==0)
                  strcpy2(NEWDEVICE.dev_rr_post_run, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"regular_run_interval")==0)
                {
                  if ((NEWDEVICE.dev_rr_interval = atoi(ask_value(NEWDEVICE.name, name, value))) <= 0)
                    startuperror("Invalid regular_run_interval for %s: %s\n", NEWDEVICE.name, value);
                }
                else
                if (strcasecmp(name,"regular_run_cmdfile")==0)
                  strcpy2(NEWDEVICE.dev_rr_cmdfile, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"regular_run_cmd")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);

                  // If not empty, buffer is terminated with double-zero.
                  if (*value)
                  {
                    p = NEWDEVICE.dev_rr_cmd;
                    while (*p)
                      p = strchr(p, 0) +1;
                    if ((ssize_t)strlen(value) <= SIZE_RR_CMD -2 -(p - NEWDEVICE.dev_rr_cmd))
                    {
                      strcpy(p, value);
                      *(p +strlen(value) +1) = 0;
                    }
                    else
                      startuperror("Not enough space for %s regular_run_cmd value: %s\n", NEWDEVICE.name, value);
                  }
                }
                else
                if (strcasecmp(name,"regular_run_logfile")==0)
                  strcpy2(NEWDEVICE.dev_rr_logfile, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"regular_run_loglevel")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);
                  NEWDEVICE.dev_rr_loglevel = set_level(NEWDEVICE.name, name, value);
                }
                else
                if (strcasecmp(name,"regular_run_statfile")==0)
                  strcpy2(NEWDEVICE.dev_rr_statfile, ask_value(NEWDEVICE.name, name, value));
	        else
                if (strcasecmp(name,"logfile")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);

                  // 3.1.12: special filename:
                  if ((p = strstr(value, "modemname")))
                  {
                    if (strlen(value) -9 +strlen(device_name) < sizeof(tmp))
                    {
                      sprintf(tmp, "%.*s%s%s", (int)(p -value), value, device_name, p +9);
                      strcpy(value, tmp);
                    }
                  }
                  strcpy2(NEWDEVICE.logfile, value);
                }
                else
                if (strcasecmp(name,"loglevel")==0)
                  NEWDEVICE.loglevel = set_level(NEWDEVICE.name, name, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"check_network")==0)
                {
                  // For backward compatibility to older version with boolean value
                  ask_value(NEWDEVICE.name, name, value);
                  if ((NEWDEVICE.check_network = yesno_check(value)) == -1)
                    NEWDEVICE.check_network=atoi(value);
                }
                else
                if (strcasecmp(name,"messageids")==0)
                  NEWDEVICE.messageids = atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"voicecall_vts_list")==0)
                {
    	          if ((NEWDEVICE.voicecall_vts_list = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"identity")==0)
                  strcpy2(NEWDEVICE.conf_identity, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"check_memory_method")==0)
                  NEWDEVICE.check_memory_method = atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"cmgl_value")==0)
                  strcpy2(NEWDEVICE.cmgl_value, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"priviledged_numbers")==0)
                {
                  ask_value(NEWDEVICE.name, name, value);

                  for (j = 1; ; j++)
                  {
                    if (j >= 25)
                    {
                      startuperror("Too many priviledged numbers in device %s.\n", NEWDEVICE.name);
                      break;
                    }

                    if (getsubparam(value, j, tmp, sizeof(tmp)))
                    {
                      // If not empty, buffer is terminated with double-zero.
                      p = NEWDEVICE.priviledged_numbers;
                      while (*p)
                        p = strchr(p, 0) +1;
                      if ((ssize_t)strlen(tmp) <= SIZE_PRIVILEDGED_NUMBERS -2 -(p - NEWDEVICE.priviledged_numbers))
                      {
                        strcpy(p, tmp);
                        *(p +strlen(tmp) +1) = 0;
                      }
                      else
                        startuperror("Not enough space for priviledged incoming numbers in device %s.\n", NEWDEVICE.name);
                    }
                    else
                      break;
                  }
                }
                else
                if (strcasecmp(name,"read_timeout")==0)
	          NEWDEVICE.read_timeout=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"ms_purge_hours")==0)
	          NEWDEVICE.ms_purge_hours=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"ms_purge_minutes")==0)
	          NEWDEVICE.ms_purge_minutes=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"ms_purge_read")==0)
                {
                  if ((NEWDEVICE.ms_purge_read = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"detect_message_routing")==0)
                {
                  if ((NEWDEVICE.detect_message_routing = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"detect_unexpected_input")==0)
                {
                  if ((NEWDEVICE.detect_unexpected_input = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"unexpected_input_is_trouble")==0)
                {
                  if ((NEWDEVICE.unexpected_input_is_trouble = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"adminmessage_limit")==0)
                  NEWDEVICE.adminmessage_limit=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"adminmessage_count_clear")==0)
                  NEWDEVICE.adminmessage_count_clear=atoi(ask_value(NEWDEVICE.name, name, value)) *60;
                else
                if (strcasecmp(name,"voicecall_ignore_modem_response")==0)
                {
    	          if ((NEWDEVICE.voicecall_ignore_modem_response = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"voicecall_hangup_ath")==0)
                {
    	          if ((NEWDEVICE.voicecall_hangup_ath = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"voicecall_vts_quotation_marks")==0)
                {
    	          if ((NEWDEVICE.voicecall_vts_quotation_marks = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"voicecall_cpas")==0)
                {
    	          if ((NEWDEVICE.voicecall_cpas = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"voicecall_clcc")==0)
                {
    	          if ((NEWDEVICE.voicecall_clcc = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"status_signal_quality")==0)
                {
    	          if ((NEWDEVICE.status_signal_quality = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"status_include_counters")==0)
                {
    	          if ((NEWDEVICE.status_include_counters = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"communication_delay")==0)
                  NEWDEVICE.communication_delay=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"hangup_incoming_call")==0)
                {
    	          if ((NEWDEVICE.hangup_incoming_call = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"max_continuous_sending")==0)
	          NEWDEVICE.max_continuous_sending=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"socket_connection_retries")==0)
	          NEWDEVICE.socket_connection_retries=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"socket_connection_alarm_after")==0)
	          NEWDEVICE.socket_connection_alarm_after=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"socket_connection_errorsleeptime")==0)
	          NEWDEVICE.socket_connection_errorsleeptime=atoi(ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"report_device_details")==0)
                {
    	          if ((NEWDEVICE.report_device_details = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"using_routed_status_report")==0)
                {
    	          if ((NEWDEVICE.using_routed_status_report = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"routed_status_report_cnma")==0)
                {
    	          if ((NEWDEVICE.routed_status_report_cnma = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"needs_wakeup_at")==0)
                {
    	          if ((NEWDEVICE.needs_wakeup_at = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"keep_messages")==0)
                {
    	          if ((NEWDEVICE.keep_messages = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"trust_spool")==0)
                {
    	          if ((NEWDEVICE.trust_spool = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"smsc_pdu")==0)
                {
    	          if ((NEWDEVICE.smsc_pdu = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"telnet_login")==0)
                  strcpy2(NEWDEVICE.telnet_login, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"telnet_login_prompt")==0)
                  strcpy2(NEWDEVICE.telnet_login_prompt, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"telnet_login_prompt_ignore")==0)
                  strcpy2(NEWDEVICE.telnet_login_prompt_ignore, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"telnet_password")==0)
                  strcpy2(NEWDEVICE.telnet_password, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"telnet_password_prompt")==0)
                  strcpy2(NEWDEVICE.telnet_password_prompt, ask_value(NEWDEVICE.name, name, value));
                else
                if (strcasecmp(name,"signal_quality_ber_ignore")==0)
                {
    	          if ((NEWDEVICE.signal_quality_ber_ignore = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"verify_pdu")==0)
                {
    	          if ((NEWDEVICE.verify_pdu = yesno_check(ask_value(NEWDEVICE.name, name, value))) == -1)
                    startuperror(yesno_error, name, value);
                }
                else
                if (strcasecmp(name,"loglevel_lac_ci")==0)
                  NEWDEVICE.loglevel_lac_ci=set_level(NEWDEVICE.name, name, value);
                else
                if (strcasecmp(name,"log_not_registered_after")==0)
	          NEWDEVICE.log_not_registered_after=atoi(ask_value(NEWDEVICE.name, name, value));
                else
	          startuperror("Unknown setting for modem %s: %s\n", NEWDEVICE.name, name);
              }
	    }

          }
        }
        else
          break;
      }
    }

    fclose(File);

    set_alarmhandler(alarmhandler, alarmlevel);

    // if loglevel is unset, then set it depending on if we use syslog or a logfile
    if (loglevel==-1)
    {
      if (logfile[0] == 0 || enable_smsd_debug)
        loglevel=LOG_DEBUG;
      else
        loglevel=LOG_WARNING;
    }

    if (conf_ask > 1)
    {
      printf("Smsd will now try to start.\n");
      fflush(stdout);
    }
  }
  else
  {
    fprintf(stderr,"Cannot open config file for read.\n");
    return 0;
  }
  return 1;
}


int getqueue(char* name, char* directory) // Name can also be a phone number
{
  int i;
  int j;
#ifdef DEBUGMSG
  printf("!! getqueue(name=%s,... )\n",name);
#endif

  // If no queues are defined, then directory is always d_checked
  if (queues[0].name[0]==0)
  {
    strcpy(directory,d_checked);
#ifdef DEBUGMSG
  printf("!! Returns -2, no queues, directory=%s\n",directory);
#endif
    return -2;
  } 
  // Short number is also accepted as a number:
  // 3.1beta4: A number can probably start with # or *:
  //if (is_number(name) || (*name == 's' && is_number(name +1)))
  if (isdigitc(*name) || (*name && strchr("#*", *name)) || (strlen(name) > 1 && *name == 's' && isdigitc(*(name +1))))
  {
#ifdef DEBUGMSG
  printf("!! Searching by number\n");
#endif
    i=0;
    while (queues[i].name[0] && (i < NUMBER_OF_MODEMS))
    {
      j=0;
      while (queues[i].numbers[j][0] && (j<NUMS))
      {
        if (!strncmp(queues[i].numbers[j],name,strlen(queues[i].numbers[j])))
	{
  	  strcpy(directory,queues[i].directory);
#ifdef DEBUGMSG
  printf("!! Returns %i, directory=%s\n",i,directory);
#endif
	  return i;
	}
	j++;
      }
      i++;
    }
  }
  else
  {
#ifdef DEBUGMSG
  printf("!! Searching by name\n");
#endif
    i=0;
    while (queues[i].name[0] && (i < NUMBER_OF_MODEMS))
    {
      if (!strcmp(name,queues[i].name))
      {
        strcpy(directory,queues[i].directory);
#ifdef DEBUGMSG
  printf("!! Returns %i, directory=%s\n",i,directory);
#endif
        return i;
      }
      i++;
    }
  }
  /* not found */
  directory[0]=0;
#ifdef DEBUGMSG
  printf("!! Returns -1, not found, name=%s, directory=%s\n", name, directory);
#endif
  return -1;
}

int getdevice(char* name)
{
  int i=0;

  while (devices[i].name[0] && (i < NUMBER_OF_MODEMS))
  {
    if (!strcmp(name,devices[i].name))
      return i;
    i++;
  }
  return -1;
}

void help()
{
  printf("smsd spools incoming and outgoing sms.\n\n");
  printf("Usage:\n");
  printf("         smsd [options]\n\n");
  printf("Options:\n");
  printf("         -a  ask config settings\n");
  printf("         -cx set config file to x\n");
  printf("         -Dx decode GSM 7bit Packed string x\n");
  printf("         -Ex encode string x to GSM 7bit Packed format\n");
  printf("         -ix set infofile to x\n");
  printf("         -px set pidfile to x\n");
  printf("         -lx set logfile to x\n");
  printf("         -nx set process name argument to x\n");
  printf("         -ux set username to x\n");
  printf("         -gx set groupname to x\n");
  printf("         -h  this help\n");
#ifndef NOSTATS
  printf("         -s  display status monitor\n");
#endif
  printf("         -t  run smsd in terminal\n");
  printf("         -C  Communicate with device\n\n");
  printf("         -V  print copyright and version\n\n");
  printf("All other options are set by the file %s.\n\n", configfile); 
  printf("Output is written to stdout, errors are written to stderr.\n\n");
  exit(0);
}

void parsearguments(int argc,char** argv)
{
  int result;
  int i;

  snprintf(configfile, sizeof(configfile), "%s", DEFAULT_CONFIGFILE);
  printstatus=0;
  arg_infofile[0] = 0;
  arg_pidfile[0] = 0;
  arg_username[0] = 0;
  arg_groupname[0] = 0;
  arg_logfile[0] = 0;
  arg_terminal = 0;
  communicate[0] = 0;
  arg_7bit_packed[0] = 0;
  do_encode_decode_arg_7bit_packed = 0;

  // 3.1.1: Start and stop options are provided by the script, not by the daemon:
  for (i = 1; i < argc; i++)
  {
    if (!strcasecmp(argv[i], "START") || !strcasecmp(argv[i], "STOP"))
    {
      printf("Invalid option for smsd: %s. Use the sms3 script (or equivalent) for start and stop operations.\n", argv[i]);
      exit(0);
    }
  }
  // --------

  do
  {
    result=getopt(argc,argv,"asthc:D:E:Vi:p:l:n:u:g:C:");
    switch (result)
    {
      case 'a': conf_ask = 1;
                break;
      case 'h': help(); 
                break;
      case 'c': copyvalue(configfile, sizeof(configfile) -1, optarg, "configfile commandline argument");
                break;
      case 's': 
#ifndef NOSTATS
                printstatus=1;
                break;
#else
                printf("Status monitor is not included in this compilation.\n");
                exit(0);
#endif
      case 't': arg_terminal = 1;
                break;
      case 'V': printf("Version %s, Copyright (c) Keijo Kasvi, %s@%s.%s, http://smstools3.kekekasvi.com\n",
                       smsd_version,"smstools3","kekekasvi","com");
                printf("Support: http://smstools3.kekekasvi.com/index.php?p=support\n");
                printf("Based on SMS Server Tools 2 from Stefan Frings, %s@%s.%s, http://www.meinemullemaus.de\n",
                       "smstools","meinemullemaus","de");
                printf("SMS Server Tools version 2 and below are Copyright (C) Stefan Frings.\n");
                exit(0);
      case 'i': copyvalue(arg_infofile, sizeof(arg_infofile) -1, optarg, "infofile commandline argument");
                break;
      case 'p': copyvalue(arg_pidfile, sizeof(arg_pidfile) -1, optarg, "pidfile commandline argument");
                break;
      case 'l': copyvalue(arg_logfile, sizeof(arg_logfile) -1, optarg, "logfile commandline argument");
                break;
      case 'n':
                // 3.1.7: This is handled in the main() code.
                break;
      case 'u': copyvalue(arg_username, sizeof(arg_username) -1, optarg, "username commandline argument");
                break;
      case 'g': copyvalue(arg_groupname, sizeof(arg_groupname) -1, optarg, "groupname commandline argument");
                break;
      case 'C': copyvalue(communicate, sizeof(communicate) -1, optarg, "Communicate commandline argument");
                break;
      case 'D':
      case 'E': snprintf(arg_7bit_packed, sizeof(arg_7bit_packed), "%s", optarg);
                do_encode_decode_arg_7bit_packed = (result == 'E') ? 1 : 2;
                break;
    }
  }
  while (result>0);
}

int check_directory(char *dir)
{
  int result = 0;
  char fname[PATH_MAX];
  int fd;
  DIR* dirdata;
  FILE *fp;

  if (dir && *dir)
  {
    if (!(dirdata = opendir(dir)))
      result = 2;
    else
    {
      closedir(dirdata);
      strcpy(fname, dir);
      if (fname[strlen(fname) -1] != '/')
        strcat(fname, "/");
      strcat(fname, "test.XXXXXX");
      if ((fd = mkstemp(fname)) == -1)
        result = 3;
      else
      {
        close(fd);
        unlink(fname);

        // 3.1.5: mkstemp creates with 600, check if with 644 or 666 can be created:
        if (!(fp = fopen(fname, "w")))
          result = 4;
        else
        {
          fclose(fp);
          unlink(fname);
        }
      }
    }
  }
  else
    result = 1;
  return result;
}

void remove_lockfiles(char *dir)
{
  DIR* dirdata;
  struct dirent* ent;
  struct stat statbuf;
  char tmpname[PATH_MAX];

  if (dir && *dir)
  {
    if ((dirdata = opendir(dir)))
    {
      while ((ent = readdir(dirdata)))
      {
        sprintf(tmpname, "%s%s%s", dir, (dir[strlen(dir) -1] != '/')? "/" : "", ent->d_name);
        stat(tmpname, &statbuf);
        if (S_ISDIR(statbuf.st_mode) == 0)
          if (strcmp(tmpname +strlen(tmpname) -5, ".LOCK") == 0)
            if (unlink(tmpname) != 0)
              startuperror("Cannot unlink file %s: %s\n", tmpname, strerror(errno));  
      }
      closedir(dirdata);
    }
  }
}

void wrlogfile(int *result, char* format, ...)
{
  va_list argp;
  char text[2048];

  va_start(argp, format);
  vsnprintf(text, sizeof(text), format, argp);
  va_end(argp);

  fprintf(stderr, "%s\n", text);
  writelogfile(LOG_CRIT, 0, "%s", text);

  if (result)
    (*result)++;
}

int startup_check(int result)
{
  // result has initial value and total number of problems is returned.

  char *msg_dir = "%s directory %s cannot be opened.";
  char *msg_file = "%s directory %s is not writable.";
  int i;
  int x;
  int y;
  FILE *fp;
  char tmp[PATH_MAX];
  char fname[PATH_MAX];
  char *p;
  char *p2;
  int d_incoming_ok = 0;
  int d_saved_ok = 0;
  struct stat statbuf;
  char timestamp[81];
  time_t now;

  if (startup_err_str)
  {
    wrlogfile(NULL, "There was %i error%s while reading the config file:", startup_err_count, (startup_err_count > 1)? "s" : "");  
    p = startup_err_str;
    while (p && *p)
    {
      if ((p2 = strchr(p, '\n')))
        *p2 = 0;
      wrlogfile(&result, "- %s", p);
      p = (p2)? p2 +1 : NULL;  
    }
    free(startup_err_str);
    startup_err_str = NULL;
    startup_err_count = 0;
  }

  // After this a lockfile errors are collected to startup_err_str. 

  if ((i = check_directory(d_spool)) == 1)
    wrlogfile(&result, "Spool (outgoing) directory definition is missing.");
  else if (i > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Spool", d_spool);
  else if (i == 0)
    remove_lockfiles(d_spool);

  if ((i = check_directory(d_stats)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Stats", d_stats);

  if ((i = check_directory(d_failed)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Failed", d_failed);
  else if (i == 0)
    remove_lockfiles(d_failed);

  if ((i = check_directory(d_incoming)) == 1)
    wrlogfile(&result, "Incoming directory definition is missing.");
  else if (i > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Incoming", d_incoming);
  else if (i == 0)
  {
    remove_lockfiles(d_incoming);
    d_incoming_ok = 1;
  }

  if (queues[0].name[0] == 0)
  {
    if ((i = check_directory(d_checked)) == 1)
      wrlogfile(&result, "Checked directory definition is missing.");
    else if (i > 1)
      wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Checked", d_checked);
    else if (i == 0)
      remove_lockfiles(d_checked);
  }

  if ((i = check_directory(d_sent)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Sent", d_sent);
  else if (i == 0)
    remove_lockfiles(d_sent);

  if ((i = check_directory(d_report)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Report", d_report);
  else if (i == 0)
    remove_lockfiles(d_report);

  if ((i = check_directory(d_phonecalls)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Phonecalls", d_phonecalls);
  else if (i == 0)
    remove_lockfiles(d_phonecalls);

  if ((i = check_directory(d_saved)) > 1)
    wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Saved", d_saved);
  else if (i == 0)
  {
    remove_lockfiles(d_saved);
    d_saved_ok = 1;
  }

  x = 0;
  while (queues[x].name[0] && (x < NUMBER_OF_MODEMS))
  {
    if ((i = check_directory(queues[x].directory)) == 1)
      wrlogfile(&result, "Queue %s directory definition is missing.", queues[x].name);
    else if (i > 1)
      wrlogfile(&result, (i == 2)? msg_dir : msg_file, "Queue", queues[x].directory);
    else if (i == 0)
    {
      remove_lockfiles(queues[x].directory);

      // 3.1.5: Check if same (similar typed) directory is used for more than one queue:
      i = 0;
      while (queues[i].name[0] && (i < NUMBER_OF_MODEMS))
      {
        if (i != x)
          if (!strcmp(queues[i].directory, queues[x].directory))
            wrlogfile(&result, "Queue %s has same directory with queue %s.", queues[i].name, queues[x].name);

        i++;
      }
    }

    // Should also check that all queue names have a provider setting too:
    //if (queues[x].numbers[0][0] == 0)
    //  wrlogfile(&result, "Queue %s has no provider number(s) defined.", queues[x].name);
    // 3.1.7: If providers are not set for the queue, use "catch-all".
    if (queues[x].numbers[0][0] == 0)
    {
      for (y = 1; ; y++)
      {
        if (getsubparam("0,1,2,3,4,5,6,7,8,9,s", y, tmp, sizeof(tmp)))
        {
          if (y > NUMS)
          {
            wrlogfile(&result, "A definition NUM is too small.");
            break;
          }
          snprintf(queues[x].numbers[y - 1], SIZE_NUM, "%s", tmp);
        }
        else
          break;
      }
    }

    // 3.1.7: Check if there are queues which are not served by any modem:
    p = 0;
    snprintf(tmp, sizeof(tmp), "Queues are used, but %s is not served by any modem.", queues[x].name);

    for (y = 0; y < NUMBER_OF_MODEMS; y++)
    {
      if (devices[y].name[0])
      {
        for (i = 0; i < NUMBER_OF_MODEMS; i++)
        {
          if (!strcmp(devices[y].queues[i], queues[x].name))
          {
            if (devices[y].outgoing)
            {
              *tmp = 0;
              break;
            }
            else
              strcat_realloc(&p, devices[y].name, (p)? ", " : "");
          }
        }
      }
    }

    if (*tmp)
    {
       if (p)
         wrlogfile(&result, "%s Modem%s %s have outgoing disabled.", tmp, (strstr(p, ","))? "s" : "", p);
       else
         wrlogfile(&result, "%s", tmp);
    }

    free(p);

    x++;
  }

  if (*eventhandler && executable_check)
  {
    if (!(fp = fopen(eventhandler, "r")))
      wrlogfile(&result, "Eventhandler %s cannot be read: %s", eventhandler, strerror(errno));
    else
    {
      fclose(fp);
      if (!is_executable(eventhandler))
        wrlogfile(&result, "Eventhandler %s is not executable for smsd.", eventhandler);
    }
  }

  if (*checkhandler && executable_check)
  {
    if (!(fp = fopen(checkhandler, "r")))
      wrlogfile(&result, "Checkhandler %s cannot be read: %s", checkhandler, strerror(errno));
    else
    {
      fclose(fp);
      if (!is_executable(checkhandler))
        wrlogfile(&result, "Checkhandler %s is not executable for smsd.", checkhandler);
    }
  }

  if (*alarmhandler && executable_check)
  {
    if (!(fp = fopen(alarmhandler, "r")))
      wrlogfile(&result, "Alarmhandler %s cannot be read: %s", alarmhandler, strerror(errno));
    else
    {
      fclose(fp);
      if (!is_executable(alarmhandler))
        wrlogfile(&result, "Alarmhandler %s is not executable for smsd.", alarmhandler);
    }
  }

  if (*regular_run && executable_check)
  {
    if (!(fp = fopen(regular_run, "r")))
      wrlogfile(&result, "Regular run %s cannot be read: %s", regular_run, strerror(errno));
    else
    {
      fclose(fp);
      if (!is_executable(regular_run))
        wrlogfile(&result, "Regular run %s is not executable for smsd.", regular_run);
    }
  }

  for (x = 0; x < NUMBER_OF_MODEMS; x++)
  {
    if (devices[x].name[0])
    {
      if (devices[x].device[0] == 0)
        wrlogfile(&result, "%s has no device specified.", devices[x].name);

#ifdef DISABLE_INET_SOCKET
      if (DEVICE_X_IS_SOCKET)
        wrlogfile(&result, "Device %s of %s specifies an inet socket, but sockets are not available in this compilation.", devices[x].device, devices[x].name);
#else
      if (DEVICE_X_IS_SOCKET)
        if (!strchr(devices[x].device, ':'))
          wrlogfile(&result, "%s has illegal internet host %s specified, must be @<host_or_ip>:<port>", devices[x].name, devices[x].device);
#endif

      if (queues[0].name[0])
        if (devices[x].queues[0][0] == 0)
          wrlogfile(&result, "Queues are used, but %s has no queue(s) defined.", devices[x].name);

      if (devices[x].eventhandler[0] && strcmp(eventhandler, devices[x].eventhandler) != 0 && executable_check)
      {
        if (!(fp = fopen(devices[x].eventhandler, "r")))
          wrlogfile(&result, "%s eventhandler %s cannot be read: %s", devices[x].name, devices[x].eventhandler, strerror(errno));
        else
        {
          fclose(fp);
          if (!is_executable(devices[x].eventhandler))
            wrlogfile(&result, "%s eventhandler %s is not executable for smsd.", devices[x].name, devices[x].eventhandler);
        }
      }

      if (devices[x].eventhandler_ussd[0] && strcmp(eventhandler, devices[x].eventhandler_ussd) != 0 && strcmp(devices[x].eventhandler, devices[x].eventhandler_ussd) != 0 && executable_check)
      {
        if (!(fp = fopen(devices[x].eventhandler_ussd, "r")))
          wrlogfile(&result, "%s eventhandler_ussd %s cannot be read: %s", devices[x].name, devices[x].eventhandler_ussd, strerror(errno));
        else
        {
          fclose(fp);
          if (!is_executable(devices[x].eventhandler_ussd))
            wrlogfile(&result, "%s eventhandler_ussd %s is not executable for smsd.", devices[x].name, devices[x].eventhandler_ussd);
        }
      }

      if (devices[x].pdu_from_file[0])
      {
        strcpy(tmp, devices[x].pdu_from_file);
        if ((p = strrchr(tmp, '/')))
        {
          *p = 0;
          if ((i = check_directory(tmp)) > 1)
            wrlogfile(&result, (i == 2)? msg_dir : msg_file, "pdu_from_file", tmp);
        }

        if (!value_in(devices[x].check_memory_method, 2, CM_NO_CPMS, CM_CPMS))
          wrlogfile(&result, "Device %s uses pdu_from_file but it can be used only with check_memory_method values %i or %i.",
                    devices[x].name, CM_NO_CPMS, CM_CPMS);
      }

      if (devices[x].dev_rr[0] && executable_check)
      {
        if (!(fp = fopen(devices[x].dev_rr, "r")))
          wrlogfile(&result, "%s regular_run file %s cannot be read: %s", devices[x].name, devices[x].dev_rr, strerror(errno));
        else
        {
          fclose(fp);
          if (!is_executable(devices[x].dev_rr))
            wrlogfile(&result, "%s regular_run file %s is not executable for smsd.", devices[x].name, devices[x].dev_rr);
        }
      }

      if (devices[x].dev_rr_post_run[0] && executable_check && strcmp(devices[x].dev_rr, devices[x].dev_rr_post_run))
      {
        if (!(fp = fopen(devices[x].dev_rr_post_run, "r")))
          wrlogfile(&result, "%s regular_run_post_run file %s cannot be read: %s", devices[x].name, devices[x].dev_rr_post_run, strerror(errno));
        else
        {
          fclose(fp);
          if (!is_executable(devices[x].dev_rr_post_run))
            wrlogfile(&result, "%s regular_run_post_run file %s is not executable for smsd.", devices[x].name, devices[x].dev_rr_post_run);
        }
      }

      if (devices[x].dev_rr_cmdfile[0])
      {
        strcpy(tmp, devices[x].dev_rr_cmdfile);
        if ((p = strrchr(tmp, '/')))
        {
          *p = 0;
          if ((i = check_directory(tmp)) > 1)
            wrlogfile(&result, (i == 2)? msg_dir : msg_file, "regular_run_cmdfile", tmp);
        }
      }

      if (devices[x].dev_rr_logfile[0])
      {
        if (!(fp = fopen(devices[x].dev_rr_logfile, "a")))
          wrlogfile(&result, "%s regular_run_logfile %s cannot be written: %s", devices[x].name, devices[x].dev_rr_logfile, strerror(errno));
        else
          fclose(fp);
      }

      if (devices[x].dev_rr_statfile[0])
      {
        if (!(fp = fopen(devices[x].dev_rr_statfile, "a")))
          wrlogfile(&result, "%s regular_run_statfile %s cannot be written: %s", devices[x].name, devices[x].dev_rr_statfile, strerror(errno));
        else
          fclose(fp);

        // Devices cannot have the same statfile because it's overwritten by each process.
        for (y = 0; y < NUMBER_OF_MODEMS; y++)
        {
          if (y == x)
            continue;
          if (devices[y].name[0])
            if (strcmp(devices[y].dev_rr_statfile, devices[x].dev_rr_statfile) == 0)
              wrlogfile(&result, "Devices %s and %s has the same regular_run_statfile %s.", devices[x].name, devices[y].name, devices[x].dev_rr_statfile);
        }
      }

      if (devices[x].messageids < 1 || devices[x].messageids > 3)
        wrlogfile(&result, "Device %s has invalid value for messageids (%i).", devices[x].name, devices[x].messageids);

      if (value_in(devices[x].check_memory_method, 5, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
        if (devices[x].cmgl_value[0] == 0)
          wrlogfile(&result, "Device %s uses check_memory_method %i but cmgl_value is not defined.", devices[x].name, devices[x].check_memory_method);

      if (!value_in(devices[x].check_memory_method, 8, CM_NO_CPMS, CM_CPMS, CM_CMGD, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
        wrlogfile(&result, "Device %s has invalid value for check_memory_method (%i).", devices[x].name, devices[x].check_memory_method);

      if (devices[x].priviledged_numbers[0])
        if (!value_in(devices[x].check_memory_method, 3, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
          wrlogfile(&result, "Device %s has priviledged_numbers defined but it can only be used with check_memory_method values %i, %i or %i.",
                    devices[x].name, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM);

      if (devices[x].read_timeout < 1)
        wrlogfile(&result, "Device %s has invalid value for read_timeout (%i).", devices[x].name, devices[x].read_timeout);

      if (devices[x].ms_purge_hours < 0)
        wrlogfile(&result, "Device %s has invalid value for ms_purge_hours (%i).", devices[x].name, devices[x].ms_purge_hours);

      if (devices[x].ms_purge_minutes < 0)
        wrlogfile(&result, "Device %s has invalid value for ms_purge_minutes (%i).", devices[x].name, devices[x].ms_purge_minutes);

      if (!value_in(devices[x].check_network, 3, 0, 1, 2))
        wrlogfile(&result, "Device %s has invalid value for check_network (%i).", devices[x].name, devices[x].check_network);

      // 3.1.7: Devices cannot have the same port.
      for (y = 0; y < NUMBER_OF_MODEMS; y++)
      {
        if (y == x)
          continue;
        if (devices[y].name[0] && devices[y].device[0])
          if (strcmp(devices[y].device, devices[x].device) == 0)
            wrlogfile(&result, "Devices %s and %s has the same port ( device = %s ).", devices[x].name, devices[y].name, devices[x].device);
      }

#ifndef USE_ICONV
      if (devices[x].ussd_convert == 1)
        wrlogfile(&result, "Device %s uses ussd_convert = 1, but it's only available when USE_ICONV is defined.", devices[x].name);
#endif

      // 3.1.12: Both cpas and clcc cannot be defined.
      if (devices[x].voicecall_cpas && devices[x].voicecall_clcc)
        wrlogfile(&result, "Devices %s has both voicecall_cpas and voicecall_clcc defined. Decide which one to use.", devices[x].name);

    }
  }

  // Administrative alerts from mainspooler:
  // If adminmessage_device is specified, it must exist and be usable.
  // Later is checked if any device is sending administrative messages and
  // if mainspooler can use this device.
  if (*adminmessage_device)
  {
    i = 0;
    for (x = 0; x < NUMBER_OF_MODEMS && !i; x++)
    {
      if (!strcmp(devices[x].name, adminmessage_device))
      {
        if (devices[x].outgoing == 0)
        {
          wrlogfile(&result, "Mainspooler uses %s to send administrative messages, but this device has outgoing disabled.", adminmessage_device);
          break;
        }

        if (devices[x].admin_to[0] || admin_to[0])
          i = 1;
        else
        {
          wrlogfile(&result, "Mainspooler uses %s to send administrative messages, but this device has no admin_to specified.", adminmessage_device);
          break;
        }
      }
    }

    if (!i)
      wrlogfile(&result, "Mainspooler has invalid adminmessage_device setting (%s): device not found.", adminmessage_device);
  }

  if (*whitelist)
  {
    if (!(fp = fopen(whitelist, "r")))
      wrlogfile(&result, "Whitelist %s cannot be read: %s", whitelist, strerror(errno));
    else
      fclose(fp);
  }

  if (*blacklist)
  {
    if (!(fp = fopen(blacklist, "r")))
      wrlogfile(&result, "Blacklist %s cannot be read: %s", blacklist, strerror(errno));
    else
      fclose(fp);
  }

  if (*infofile)
  {
    if (!(fp = fopen(infofile, "w")))
      wrlogfile(&result, "Infofile %s cannot be created: %s", infofile, strerror(errno));
    else
      fclose(fp);
    unlink(infofile);
  }

  if (store_received_pdu < 0 || store_received_pdu > 3)
    wrlogfile(&result, "Invalid value for store_received_pdu.");

  if (store_sent_pdu < 0 || store_sent_pdu > 3)
    wrlogfile(&result, "Invalid value for store_sent_pdu.");

  if (ic_purge_hours < 0)
    wrlogfile(&result, "Invalid value for ic_purge_hours (%i).", ic_purge_hours);

  if (ic_purge_minutes < 0)
    wrlogfile(&result, "Invalid value for ic_purge_minutes (%i).", ic_purge_minutes);

  if (ic_purge_interval < 0)
    wrlogfile(&result, "Invalid value for ic_purge_interval (%i).", ic_purge_interval);

  if (smart_logging)
  {
    if (logfile[0] == 0 || strcmp(logfile, "syslog") == 0 || strcmp(logfile, "0") == 0)
      wrlogfile(&result, "Smart logging cannot be used when syslog is used for logging.");

    for (x = 0; x < NUMBER_OF_MODEMS; x++)
      if (devices[x].name && devices[x].logfile[0])
        if (strcmp(devices[x].logfile, "syslog") == 0 || strcmp(devices[x].logfile, "0") == 0)
          wrlogfile(&result, "Smart logging cannot be used when syslog is used for logging, device %s.", devices[x].name);
  }

  if (executable_check)
  {
    if (!is_executable(shell))
      wrlogfile(&result, "Shell %s does not exist or is not executable for smsd.", shell);
    else if (shell_test)
    {
      char *error = 0;
      char tmp_data[PATH_MAX];
      char tmp_script[PATH_MAX];
      char tmp[PATH_MAX +PATH_MAX];
      int fd;
      int i;

      sprintf(tmp_data, "%s/smsd_data.XXXXXX", "/tmp");
      if ((fd = mkstemp(tmp_data)) == -1)
        error = "Cannot create test data file.";
      else
      {
        close(fd);

        // 3.1.14: Use incoming directory instead of /tmp which may be mounted noexec:
        sprintf(tmp_script, "%s/smsd_script.XXXXXX", d_incoming);

        if ((fd = mkstemp(tmp_script)) == -1)
          error = "Cannot create test script file.";
        else
        {
          snprintf(tmp, sizeof(tmp), "#!%s\necho OK > \"$1\"\nexit 0\n", shell);
          if (write(fd, tmp, strlen(tmp)) < (ssize_t)strlen(tmp))
            error = "Cannot write to test script file.";
          close(fd);

          if (!error)
          {
            snprintf(tmp, sizeof(tmp), "%s %s", tmp_script, tmp_data);
            chmod(tmp_script, 0700);
            i = my_system(tmp, "startup_check (shell)");
            if (i)
              error = "Failed to execute test script.";
            else
            {
              if ((fd = open(tmp_data, O_RDONLY)) < 0)
                error = "Cannot read test data file.";
              else
              {
                read(fd, tmp, sizeof(tmp));
                if (strncmp(tmp, "OK", 2))
                  error = "Did not work.";
                close(fd);
              }
            }
          }
          unlink(tmp_script);
        }
        unlink(tmp_data);
      }

      if (error)
        wrlogfile(&result, "Shell %s testing failed: %s", shell, error);
    }
  }

  // Format strings for strftime:
  // Not much can be checked, only if it's completelly wrong...
  time(&now);

  if (!strchr(datetime_format, '%') || strftime(timestamp, sizeof(timestamp), datetime_format, localtime(&now)) == 0 || !strcmp(timestamp, datetime_format))
    wrlogfile(&result, "Format string datetime is completelly wrong: \"%s\"", datetime_format);

  if (!strchr(logtime_format, '%') || strftime(timestamp, sizeof(timestamp), logtime_format, localtime(&now)) == 0 || !strcmp(timestamp, logtime_format))
    wrlogfile(&result, "Format string logtime_format is completelly wrong: \"%s\"", logtime_format);

  if (!strchr(date_filename_format, '%') || strftime(timestamp, sizeof(timestamp), date_filename_format, localtime(&now)) == 0 || !strcmp(timestamp, date_filename_format))
    wrlogfile(&result, "Format string date_filename_format is completelly wrong: \"%s\"", date_filename_format);

  if (filename_preview < 0 || filename_preview >= SIZE_FILENAME_PREVIEW)
    wrlogfile(&result, "Value for filename_preview is illegal: \"%d\". It can be 1 ... %u.", filename_preview, SIZE_FILENAME_PREVIEW - 1);

  if (startup_err_str)
  {
    wrlogfile(NULL, "There was %i error%s while removing .LOCK files.", startup_err_count, (startup_err_count > 1)? "s" : "");  
    p = startup_err_str;
    while (p && *p)
    {
      if ((p2 = strchr(p, '\n')))
        *p2 = 0;
      wrlogfile(&result, "- %s", p);
      p = (p2)? p2 +1 : NULL;  
    }
    free(startup_err_str);
    startup_err_str = NULL;
    startup_err_count = 0;
  }

  if (d_incoming_ok && d_saved_ok)
  {
    // 3.1beta7: Search concatenation files from incoming directory.
    // If zero sized files found, they can be removed.
    // If files with data found, they can be moved to d_saved directory.
    // Existing zero sized file is overwritten, but a file containing data produces fatal error.

    for (x = 0; x < NUMBER_OF_MODEMS; x++)
    {
      if (devices[x].name[0])
      {
        sprintf(fname, CONCATENATED_DIR_FNAME, d_incoming, devices[x].name);
        if (stat(fname, &statbuf) == 0)
        {
          if (statbuf.st_size == 0)
          {
            if (unlink(fname) != 0)
              startuperror("Cannot unlink concatenation storage %: %s\n", fname, strerror(errno));
          }
          else
          {
            i = 1;
            sprintf(tmp, CONCATENATED_DIR_FNAME, d_saved, devices[x].name);
            if (stat(tmp, &statbuf) == 0)
            {
              if (statbuf.st_size != 0)
              {
                i = 0;
                wrlogfile(&result, "Concatenation storage of %s cannot be moved from incoming to saved directory, "
                                   "destination exists and has also some data ", devices[x].name);
              }
            }

            if (i)
            {
              if (movefile(fname, d_saved))
              {
                // movefile does not inform if removing source file failed:
                if (stat(fname, &statbuf) == 0)
                {
                  if (unlink(fname) != 0)
                  {
                    startuperror("Failed to move concatenation storage, cannot unlink source file %: %s\n", fname, strerror(errno));
                    i = 0;
                  }
                }

                if (i)
                  writelogfile(LOG_WARNING, 0, "Moved concatenation storage of %s from incoming to saved directory", devices[x].name);
              }
              else
                wrlogfile(&result, "Failed to move concatenation storage of %s from incoming to saved directory.", devices[x].name);
            }
          }
        }
      }
    }
  }

  if (result > 0)
  {
    wrlogfile(NULL, "There was %i major problem%s found.", result, (result > 1)? "s" : "");
    fprintf(stderr, "Cannot start. See the log file for details.\n");
  }
  else
  {
    // Report some settings:
    char buffer[PATH_MAX];
    mode_t mode;
    mode_t m;

    // 3.1.7: Mask can be set:
    // mode = umask(0);
    // umask(mode);
    if (!conf_umask)
    {
      mode = umask(0);
      umask(mode);
    }
    else
    {
      umask(conf_umask);
      mode = umask(conf_umask); // Fixed in 3.1.9.
    }

    m = 0666 & ~mode;
    sprintf(buffer, "File mode creation mask: 0%o (0%o, %c%c%c%c%c%c%c%c%c).", (int)mode, (int)m,
            (m & 0x100)? 'r':'-', (m & 0x80)? 'w':'-', (m & 0x40)? 'x':'-',
            (m & 0x20)? 'r':'-', (m & 0x10)? 'w':'-',  (m & 0x8)? 'x':'-',
            (m & 0x4)? 'r':'-', (m & 0x2)? 'w':'-', (m & 0x1)? 'x':'-');
    writelogfile0(LOG_WARNING, 0, buffer);
#ifdef DEBUGMSG
  printf("!! %s\n", buffer);
#endif

    if (validity_period < 255)
    {
      report_validity(tmp, validity_period);
      sprintf(buffer, "Default validity period is set to %s.", tmp);
      writelogfile0(LOG_WARNING, 0, buffer);
#ifdef DEBUGMSG
  printf("!! %s\n", buffer);
#endif
    }

    if (*international_prefixes)
    {
      p = international_prefixes;
      *tmp = 0;
      do
      {
        if (*tmp)
          strcat(tmp, ",");
        strcat(tmp, p);
        p += strlen(p) +1;
      }
      while (*p);

      sprintf(buffer, "Using international prefixes: %s", tmp);
      writelogfile0(LOG_WARNING, 0, buffer);
    }

    if (*national_prefixes)
    {
      p = national_prefixes;
      *tmp = 0;
      do
      {
        if (*tmp)
          strcat(tmp, ",");
        strcat(tmp, p);
        p += strlen(p) +1;
      }
      while (*p);

      sprintf(buffer, "Using national prefixes: %s", tmp);
      writelogfile0(LOG_WARNING, 0, buffer);
    }

    if (*priviledged_numbers)
    {
      sprintf(buffer, "Global priviledged_numbers: ");
      p = priviledged_numbers;
      while (*p)
      {
        if (p != priviledged_numbers)
          strcat(buffer, ",");
        strcat(buffer, p);
        p = strchr(p, 0) +1;
      }
      writelogfile0(LOG_WARNING, 0, buffer);

      // Check and report if global value is used or not.
      // Not an error even if it's not used.
      *tmp = 0;
      for (x = 0; x < NUMBER_OF_MODEMS; x++)
        if (devices[x].name[0] && devices[x].priviledged_numbers[0] == 0)
          if (value_in(devices[x].check_memory_method, 3, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))
            sprintf(strchr(tmp, 0), "%s%s", (*tmp)? "," : "", devices[x].name);

      if (*tmp)
        writelogfile(LOG_WARNING, 0, "Devices using global priviledged_numbers: %s", tmp);
      else
        writelogfile(LOG_WARNING, 0, "Note that no any device is using global priviledged_numbers.");
    }

    if (*adminmessage_device)
      writelogfile(LOG_WARNING, 0, "Mainspooler uses %s to send administrative messages.", adminmessage_device);
    else
    {
      // Check if any device is sending administrative messages.
      // If one found, it can be used by mainspooler if shared memory is availalbe.
      *tmp = 0;

      for (x = 0; x < NUMBER_OF_MODEMS; x++)
      {
        if (devices[x].name[0])
        {
          if ((devices[x].admin_to[0] || admin_to[0]) && devices[x].outgoing)
          {
            strcpy(tmp, devices[x].name);
            break;
          }
        }
      }

      if (*tmp)
      {
#ifndef NOSTATS
        strcpy(adminmessage_device, tmp);
        writelogfile(LOG_WARNING, 0, "Mainspooler will use %s to send administrative messages.", adminmessage_device);
#else
        writelogfile(LOG_WARNING, 0, "Note that at least %s will send administrative messages, but mainspooler will not because shared memory is not available.", tmp);
#endif
      }
    }

    if (strstr(smsd_version, "beta"))
    {
	if (queues[0].name[0])
	{
	        writelogfile(LOG_WARNING, 0, "Queue definitions:", tmp);
		for (x = 0; ; x++)
		{
			if (queues[x].name[0])
			{
				*tmp = 0;
				for (y = 0; y < NUMS; y++)
				{
					if (queues[x].numbers[y][0] == 0)
						break;
					sprintf(strchr(tmp, 0), "%s%s", (*tmp)? "," : "", queues[x].numbers[y]);
				}
			        writelogfile(LOG_WARNING, 0, "%s \"%s\" %s", queues[x].name, tmp, queues[x].directory);
			}
			else
				break;
		}
	}
    }

  }

  return result;
}
