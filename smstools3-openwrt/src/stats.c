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
#include <limits.h>
#include "alarm.h"
#include "smsd_cfg.h"
#include "stats.h"
#include <syslog.h>
#include "logging.h"
#include "modeminit.h"
#include "extras.h"
#include <errno.h>
#ifndef NOSTATS
#include <mm.h>
#endif

char newstatus[NUMBER_OF_MODEMS +1] = {0};
char oldstatus[NUMBER_OF_MODEMS +1] = {0};

char *statistics_current_version = "VERSION 3.1.5-1";

void initstats()
{
  int i;

// if the mm Library is not available the statistic funktion does not work.
// Use unshared memory instead disabling all statistc related functions.
// This is much easier to program.
#ifndef NOSTATS
  // 3.1.5: get rid of tempnam:
  //MM_create(DEVICES*sizeof(_stats),tempnam(0,0));

  char filename[PATH_MAX];

  // mm library also defaults to /tmp directory with pid,
  // .sem will be added to name inside mm.
  sprintf(filename, MM_CORE_FNAME, (int)getpid());

  // 3.1.5: return value is now checked:
  if (MM_create(NUMBER_OF_MODEMS *sizeof(_stats) +SIZE_SHARED_BUFFER, filename) == 0)
  {
    writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot create shared memory for statistics."));
    alarm_handler0(LOG_ERR, tb);
    exit(EXIT_FAILURE);
  }
#endif

  for (i = 0; i < NUMBER_OF_MODEMS; i++)
  {
#ifndef NOSTATS
    if ((statistics[i]=(_stats*)MM_malloc(sizeof(_stats))))
#else
    if ((statistics[i]=(_stats*)malloc(sizeof(_stats))))
#endif
    {
      statistics[i]->succeeded_counter = 0;
      statistics[i]->failed_counter = 0;
      statistics[i]->received_counter = 0;
      statistics[i]->multiple_failed_counter = 0;
      statistics[i]->status = '-';
      statistics[i]->usage_s = 0;
      statistics[i]->usage_r = 0;
      statistics[i]->message_counter = 0;
      statistics[i]->last_init = 0;
      statistics[i]->ssi = -1;
      statistics[i]->ber = -1;
    }
    else
    {
      writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot reserve memory for statistics."));
      alarm_handler0(LOG_ERR, tb);
#ifndef NOSTATS
      MM_destroy();
#endif
      exit(EXIT_FAILURE);
    }
  }

#ifndef NOSTATS
  if ((shared_buffer = (char *)MM_malloc(SIZE_SHARED_BUFFER)))
#else
  if ((shared_buffer = (char *)malloc(SIZE_SHARED_BUFFER)))
#endif
    *shared_buffer = 0;
  else
  {
    writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot reserve memory for shared buffer."));
    alarm_handler0(LOG_ERR, tb);
#ifndef NOSTATS
    MM_destroy();
#endif
    exit(EXIT_FAILURE);
  }

  rejected_counter=0;
  start_time=time(0);
  last_stats=time(0);
}

void resetstats()
{
  int i;

  for (i = 0; i < NUMBER_OF_MODEMS; i++)
  {
    statistics[i]->succeeded_counter = 0;
    statistics[i]->failed_counter = 0;
    statistics[i]->received_counter = 0;
    statistics[i]->multiple_failed_counter = 0;
    statistics[i]->status = '-';
    statistics[i]->usage_s = 0;
    statistics[i]->usage_r = 0;

    // message_counter remains untouched.

    // last_init and signal quality remains untouched.

  }

  rejected_counter = 0;
  start_time = time(0);
  last_stats = time(0);
}

void savestats()
{
  char filename[PATH_MAX];
  FILE *fp;
  int i;
  time_t now;

  if (d_stats[0] && stats_interval)
  {
    now = time(0);
    sprintf(filename, "%s/stats.tmp", d_stats);
    if ((fp = fopen(filename, "w")))
    {
      fwrite(statistics_current_version, strlen(statistics_current_version) +1, 1, fp);

      fwrite(&now, sizeof(now), 1, fp);
      fwrite(&start_time, sizeof(start_time), 1, fp);
      for (i = 0; i < NUMBER_OF_MODEMS; i++)
        fwrite(statistics[i], sizeof(_stats), 1, fp);
      fclose(fp);
    }
    else
    {
      writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot write tmp file for statistics. %s %s", filename, strerror(errno)));
      alarm_handler0(LOG_ERR, tb);
    }
  }
}

void loadstats()
{
  char filename[PATH_MAX];
  FILE *fp;
  int i;
  time_t saved_time;
  char tmp[81];

  if (d_stats[0] && stats_interval)
  {
    sprintf(filename, "%s/stats.tmp", d_stats);
    if ((fp = fopen(filename,"r")))
    {
      fread(tmp, strlen(statistics_current_version) +1, 1, fp);

      if (strncmp(statistics_current_version, tmp, strlen(statistics_current_version)))
        writelogfile0(LOG_ERR, 0, tb_sprintf("Not loading statistics tmp file because version has changed."));
      else
      {
        fread(&saved_time, sizeof(time_t), 1, fp);
        fread(&start_time, sizeof(time_t), 1, fp);
        start_time = time(0) -(saved_time -start_time);

        for (i = 0; i < NUMBER_OF_MODEMS; i++)
        {
          fread(statistics[i], sizeof(_stats), 1, fp);

          statistics[i]->status = '-';
          statistics[i]->last_init = 0;
          statistics[i]->ssi = -1;
          statistics[i]->ber = -1;
        }
      }
      fclose(fp);
    }
  }    
}

void print_status()
{
  int j;

  if (printstatus)
  {
    strcpy(oldstatus,newstatus);
    for (j = 0; j < NUMBER_OF_MODEMS; j++)
      newstatus[j]=statistics[j]->status;
    newstatus[NUMBER_OF_MODEMS] = 0;
    if (strcmp(oldstatus,newstatus))
    {
      printf("%s\n",newstatus);
    }
  }
}

void checkwritestats()
{
  time_t now;
  time_t next_time;
  char filename[PATH_MAX];
  char s[20];
  FILE* datei;
  int i;
  int sum_counter;

  if (d_stats[0] && stats_interval)
  {
    next_time=last_stats+stats_interval;
    next_time=stats_interval*(next_time/stats_interval);  // round value
    now=time(0);
    if (now>=next_time) // reached timepoint of next stats file?
    {
      // Check if there was activity if user does not want zero-files 
      if (stats_no_zeroes)
      {
        sum_counter=rejected_counter;
	for (i = 0; i < NUMBER_OF_MODEMS; i++)
	{
          if (devices[i].name[0])
          {
            sum_counter+=statistics[i]->succeeded_counter;
	    sum_counter+=statistics[i]->failed_counter;
	    sum_counter+=statistics[i]->received_counter;
	    sum_counter+=statistics[i]->multiple_failed_counter;
          }
        }
        if (sum_counter==0)
        {
          resetstats();
          last_stats=now;
          return;
        }
      }

      last_stats=time(0);
      // %Y used instead of %y to avoid compiler warning message in some environments.
      strftime(s,sizeof(s),"%Y%m%d.%H%M%S",localtime(&next_time));
      strcpyo(s, s +2);
      syslog(LOG_INFO,"Writing stats file %s",s);
      strcpy(filename,d_stats);
      strcat(filename,"/");
      strcat(filename,s);
      datei=fopen(filename,"w");
      if (datei)
      {
        fprintf(datei,"runtime,rejected\n");

	//fprintf(datei,"%li,%i\n\n", now -start_time, rejected_counter);
	fprintf(datei, "%lld,%i\n\n", (long long int)now -start_time, rejected_counter);

	fprintf(datei,"name,succeeded,failed,received,multiple_failed,usage_s,usage_r\n");
	for (i = 0; i < NUMBER_OF_MODEMS; i++)
	{
	  if (devices[i].name[0])
  	    fprintf(datei,"%s,%i,%i,%i,%i,%i,%i\n",
	      devices[i].name,
	      statistics[i]->succeeded_counter,
	      statistics[i]->failed_counter,
	      statistics[i]->received_counter,
	      statistics[i]->multiple_failed_counter,
	      statistics[i]->usage_s,
	      statistics[i]->usage_r);
	}
        fclose(datei);
	resetstats();
	last_stats=now;
      }
      else
      {
        writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot write statistic file. %s %s",filename,strerror(errno)));
        alarm_handler0(LOG_ERR, tb);
      }
    }
  }
}

// 3.1.1:
void update_message_counter(int messages, char *modemname)
{
  char filename[PATH_MAX];
  FILE *fp;
  char temp[256];
  int counter = 0;
  char *p;

  if (*d_stats)
  {
    sprintf(filename, "%s/%s.counter", d_stats, modemname);

    if ((fp = fopen(filename, "r")))
    {
      if (fgets(temp, sizeof(temp), fp))
      {
        if (!(p = strchr(temp, ':')))
          p = temp;
        else
          p++;
        counter = atoi(p);
      }
      fclose(fp);
    }

    // 3.1.7: always create a file

    counter += messages;

    if ((fp = fopen(filename, "w")))
    {
      fprintf(fp, "%s: %i\n", modemname, counter);
      fclose(fp);
    }

    STATISTICS->message_counter = counter;
  }
}

// 3.1.5:
void write_status()
{
#ifndef NOSTATS
  int i;
  char fname_tmp[PATH_MAX];
  char fname[PATH_MAX];
  FILE *fp;
  char *status;
  char buffer[256];
  time_t t;
  static size_t longest_modemname = 0;
  int include_counters;

  if (!printstatus && d_stats[0])
  {
    strcpy(oldstatus, newstatus);
    for (i = 0; i < NUMBER_OF_MODEMS; i++)
      newstatus[i] = statistics[i]->status;
    newstatus[NUMBER_OF_MODEMS] = 0;

    if (strcmp(oldstatus, newstatus))
    {
      sprintf(fname_tmp, "%s/status.tmp", d_stats);
      if ((fp = fopen(fname_tmp, "w")))
      {
        if (!longest_modemname)
          for (i = 0; i < NUMBER_OF_MODEMS; i++)
            if (devices[i].name[0])
              if (strlen(devices[i].name) > longest_modemname)
                longest_modemname = strlen(devices[i].name);

        t = time(0);
        strftime(buffer, sizeof(buffer), datetime_format, localtime(&t));
        fprintf(fp, "Status:%s\t%s,\t%s\n", (longest_modemname >= 7)? "\t" : "", buffer, newstatus);

        for (i = 0; i < NUMBER_OF_MODEMS; i++)
        {
          if (devices[i].name[0] == 0)
            continue;
          switch (newstatus[i])
          {
            case 's':
              status = "Sending";  
              break;

            case 'r':
              status = "Receiving";
              break;

            case 'i':
              status = "Idle";
              break;

            case 'b':
              status = "Blocked";
              break;

            case 't':
              status = "Trouble";
              break;

            default:
              status = "Unknown";
          }

          if (statistics[i]->last_init)
            strftime(buffer, sizeof(buffer), datetime_format, localtime(&(statistics[i]->last_init)));
          else
            strcpy(buffer, "-");

          fprintf(fp, "%s:%s\t%s,\t%s", devices[i].name, (strlen(devices[i].name) < 7 && longest_modemname >= 7)? "\t" : "", buffer, status);

          include_counters = 0;
          if (devices[i].status_include_counters == 1 ||
              (devices[i].status_include_counters == -1 && status_include_counters))
            include_counters = 1;

          if (include_counters || statistics[i]->ssi >= 0)
            fprintf(fp, ",%s", (strlen(status) < 7)? "\t" : "");

          if (include_counters)
            fprintf(fp, "\t%i,\t%i,\t%i", statistics[i]->message_counter, statistics[i]->failed_counter, statistics[i]->received_counter);

          if (statistics[i]->ssi >= 0)
          {
            if (include_counters)
              fprintf(fp, ",");

            explain_csq_buffer(buffer, 1, statistics[i]->ssi, statistics[i]->ber, devices[i].signal_quality_ber_ignore);
            fprintf(fp, "\t%s", buffer);
          }

          fprintf(fp, "\n");
        }  

        fclose(fp);

        sprintf(fname, "%s/status", d_stats);
        rename(fname_tmp, fname);
      }
    }
  }
#endif
}  
