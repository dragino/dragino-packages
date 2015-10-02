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

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "whitelist.h"
#include "extras.h"
#include "logging.h"
#include "alarm.h"
#include "smsd_cfg.h"

/* Used with >= 3.1x */
int inwhitelist_q(char* msisdn, char *queuename)
{
  FILE* file;
  char line[256];
  char* posi;
  char current_queue[32];
  int result = 1;
  int i;

  if (whitelist[0]) // is a whitelist file specified?
  {
    file=fopen(whitelist,"r");
    if (file)
    {
      *current_queue = 0;
      result = 0;
      while (fgets(line,sizeof(line),file))
      {
        posi=strchr(line,'#');     // remove comment
        if (posi)
          *posi=0;
        cutspaces(line);
        i = strlen(line);
        if (i > 0)
        {
          if (line[0] == '[' && line[i -1] == ']' && (size_t)(i -2) < sizeof(current_queue))
          {
            line[i -1] = 0;
            strcpy(current_queue, line +1);
          }
          else
          if (strncmp(msisdn,line,strlen(line))==0)
          {
            result = 1;
            break;
          }  
          else if (msisdn[0]=='s' && strncmp(msisdn+1,line,strlen(line))==0)
          {
            result = 1;
            break;
          }
        }  
      }  
      fclose(file);
      if (result == 1 && *current_queue && !(*queuename))
        strcpy(queuename, current_queue);
    }  
    else
    {
      writelogfile0(LOG_CRIT, 0, tb_sprintf("Stopping. Cannot read whitelist file %s.",whitelist));
      alarm_handler0(LOG_CRIT, tb);
      abnormal_termination(1);
    }
  }      

  return result;
}
