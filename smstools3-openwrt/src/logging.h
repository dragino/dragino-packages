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

#ifndef LOGGING_H
#define LOGGING_H

int trouble_logging_started;

int change_loglevel(int new_level);
void restore_loglevel();
int get_loglevel();

int openlogfile(char *filename, int facility, int level);

// if filename if 0, "" or "syslog": opens syslog. Level is ignored.
// else: opens a log file. Facility is not used. Level specifies the verbosity (9=highest).
// If the filename is a number it is interpreted as the file handle and 
// duplicated. The file must be already open. 
// Returns the file handle to the log file.


void closelogfile();
void writelogfile0(int severity, int trouble, char *text);
void writelogfile(int severity, int trouble, char* format, ...);
void flush_smart_logging();

#endif
