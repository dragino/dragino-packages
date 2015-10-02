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

#ifndef CFGFILE_H
#define CFGFILE_H

#include <stdio.h>

/*  Gets a single parameter from a list of parameters wich uses colons
to separate them. Returns 1 if successful. */

int getsubparam_delim(char*  parameter,
                      int n,
                      char*  subparam,  int size_subparam, 
                      char delim);

int getsubparam(char*  parameter, 
                int n, 
                char*  subparam,  int size_subparam);


/* Searches for a section [name] in a config file and goes to the
   next line. Return 1 if successful. */
   
int gotosection(FILE* file,  char*  name);



/* Reads the next line from a config file beginning at the actual position.
   Returns 1 if successful. If the next section or eof is encountered it 
   returns 0. If the file contains syntax error it returns -1 and the wrong
   line in value.*/

int my_getline(FILE* file, 
            char*  name,  int size_name,
	    char*  value,  int size_value);
	    
#endif
