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
#include "blacklist.h"
#include "extras.h"
#include "logging.h"
#include "alarm.h"
#include "smsd_cfg.h"

int inblacklist(char* msisdn)
{
  FILE* file;
  char line[256];
  char* posi;

  if (blacklist[0]) // is a blacklist file specified?
  {
    file=fopen(blacklist,"r");
    if (file)
    {
      while (fgets(line,sizeof(line),file))
      {
        posi=strchr(line,'#');     // remove comment
        if (posi)
          *posi=0;
        cutspaces(line);
	if (strlen(line)>0)
        {
          if (strncmp(msisdn,line,strlen(line))==0)
	  {
	    fclose(file);
            return 1;
    	  }  
          else if (msisdn[0]=='s' && strncmp(msisdn+1,line,strlen(line))==0)
          {
            fclose(file);
            return 1;
	  }
        }
      }  
      fclose(file);
    }  
    else
    {
      writelogfile0(LOG_CRIT, 0, tb_sprintf("Stopping. Cannot read blacklist file %s.", blacklist));
      alarm_handler0(LOG_CRIT, tb);
      abnormal_termination(1);
    }
  }      
  return 0;
}
