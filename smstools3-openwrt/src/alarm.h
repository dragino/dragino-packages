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

#ifndef ALARM_H
#define ALARM_H


// Note: Use either the devicename in set_alarmhandler OR alarm_handler but not in both.
// Set the unused parameter to "".

// Initialize some variables before using alarm_handler
void set_alarmhandler(char* handler,int level);

// calls the alarm handler
void alarm_handler0(int severity, char *text);
void alarm_handler(int severity, char* format, ...);

#endif
