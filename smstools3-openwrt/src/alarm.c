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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include "alarm.h"
#include "extras.h"
#include "smsd_cfg.h"

char* _alarmhandler={0};
int _alarmlevel=LOG_WARNING;

void set_alarmhandler(char* handler,int level)
{
  _alarmhandler=handler;
  _alarmlevel=level;
}

void alarm_handler0(int severity, char *text)
{
  alarm_handler(severity, "%s", text);
}

void alarm_handler(int severity, char* format, ...)
{
  va_list argp;
  char text[1024];
  char cmdline[PATH_MAX+1024];
  char timestamp[40];

  if (_alarmhandler[0])
  {
    va_start(argp,format);
    vsnprintf(text,sizeof(text),format,argp);
    va_end(argp);
    if (severity<=_alarmlevel)
    {
      make_datetime_string(timestamp, sizeof(timestamp), 0, 0, logtime_format);
      snprintf(cmdline,sizeof(cmdline),"%s ALARM %s %i %s \"%s\"",_alarmhandler,timestamp,severity, process_title, text);
      my_system(cmdline, "alarmhandler");
    }  
  }
}

