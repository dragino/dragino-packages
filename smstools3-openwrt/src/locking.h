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

#ifndef LOCKING_H
#define LOCKING_H

/* Locks a file and returns 1 if successful */

int lockfile( char*  filename);


/* Checks, if a file is locked */

int islocked( char*  filename);


/* Unlocks a file */

int unlockfile( char*  filename);

#endif
