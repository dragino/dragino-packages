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

#ifndef STATS_H
#define STATS_H

#include <time.h>

typedef struct
{
  int succeeded_counter;        // Number of sent SM
  int failed_counter;           // Number of not sent SM (ERROR from modem)
  int received_counter;         // Number of received SM
  int multiple_failed_counter;  // Number of multiple failed SM (3 consecutive ERROR's from modem)
  int usage_s;			// Modem usage to send SM in seconds
  int usage_r;			// Modem usage to receive SM in seconds
  char status;			// s=send r=receive i=idle b=blocked -=not running t=trouble
  int message_counter;          // This is a number of sent messages, stored in GSM1.counter file.
                                // Note: first 3 counters are cleared every time when stats_interval is reached.
  time_t last_init;
  int ssi;
  int ber;
} _stats;

_stats* statistics[NUMBER_OF_MODEMS]; // Statistic data (shared memory!)
int rejected_counter;		// Statistic counter, rejected SM, number does not fit into any queue
time_t start_time;		// Start time of smsd, allows statistic functions
int printstatus;		// if 1 smsd outputs status on stdout
time_t last_stats;		// time when the last stats file was created
char d_stats[PATH_MAX];		// path to statistic files
int stats_interval;		// time between statistic files in seconds.
int stats_no_zeroes;		// Suppress files that contain only zeroes
int status_interval;            // time between updating status file in seconds

char *shared_buffer;

/* Creates shared memory variables for statistic data */

void initstats();

/* Resets statistic data to 0 */

void resetstats();

/* saves the current statistic counter into a tmp file */

void savestats();

/* load the statistic counter from the tmp file */

void loadstats();

/* Prints modem status to stdout */

void print_status();

/* Checks if next statistic file should be written and writes it */

void checkwritestats();

void update_message_counter(int messages, char *modemname);

void write_status();

#endif // STATS_H
