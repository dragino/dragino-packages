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

#ifndef EXTRAS_H
#define EXTRAS_H

#include <stdio.h>

/* Converts a string to a boolean value. The string can be:
   1=  true, yes, on, 1
   0 = all other strings
   Only the first character is significant. */
		
int yesno(char *value);

/* Like yesno, but defaults to -1. 0 = false, no, off, 0 */
int yesno_check(char *value);

/* removes all ctrl chars */
char *cut_ctrl(char* message);

char *cut_crlf(char *st);

/* Is a character a space or tab? */
int is_blank(char c);
int line_is_blank(char *line);

/* Moves a file into another directory. Returns 1 if success. */
int movefile(char* filename, char* directory);

/* Moves a file into another directory. Destination file is protected with
   a lock file during the operation. Returns 1 if success. */
//int movefilewithdestlock(char* filename, char* directory);
int movefilewithdestlock_new(char* filename, char* directory, int keep_fname, int store_original_fname, char *prefix, char *newfilename);

/* removes ctrl chars at the beginning and the end of the text and removes */
/* \r in the text. Returns text.*/
char *cutspaces(char *text);

/* removes all empty lines */
char *cut_emptylines(char *text);

/* Checks if the text contains only numbers. */
int is_number(char* text);

int getpdufile(char *filename);

/* Gets the first file that is not locked in the directory. Returns 0 if 
   there is no file. Filename is the filename including the path. 
   Additionally it cheks if the file grows at the moment to prevent
   that two programs acces the file at the same time. */
int getfile(int trust_directory, char* dir, char* filename, int lock);

/* Replacement for system() wich can be breaked. See man page of system() */
int my_system(char *command, char *info);

/* Create and remove a PID file */
int write_pid(char* filename);
int check_pid(char *filename);
void remove_pid(char* filename);

/* Parse validity value string */
int parse_validity(char *value, int defaultvalue);
int report_validity(char *buffer, int validity_period);

/* Return a random number between 1 and toprange */
int getrand(int toprange);

/* Check permissions of filename */
int is_executable(char *filename);
int check_access(char *filename);

int value_in(int value, int arg_count, ...);

// t_sleep returns 1 if terminate is set to 1 while sleeping:
int t_sleep(int seconds);

int usleep_until(unsigned long long target_time);

unsigned long long time_usec();

int make_datetime_string(char *dest, size_t dest_size, char *a_date, char *a_time, char *a_format);

void strcat_realloc(char **buffer, char *str, char *delimiter);

char *strcpyo(char *dest, const char *src);

void getfield(char* line, int field, char* result, int size);

#endif
