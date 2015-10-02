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

#include "cfgfile.h"
#include "extras.h"
#include <sys/types.h>
#include <limits.h>
#include <string.h>

void cutcomment(char*  text)
{
  int laenge;

  // 3.1.5: Only whole line comments are allowed:
  //cp=strchr(text,'#');
  //if (cp!=0)
  //  *cp=0;
  while (is_blank(*text))
    strcpyo(text, text +1);
  if (*text == '#')
    *text = 0;

  laenge=strlen(text);
  // 3.1beta7: this was dropping scandinavic characters, unsigned test added:
  while (laenge > 0 && ((unsigned char)text[laenge -1] <= (unsigned char)' '))
  {
    text[laenge-1]=0;
    laenge--;
  }
}

int getsubparam_delim(char*  parameter, 
                      int n, 
                      char*  subparam,  int size_subparam,
                      char delim)
{
  int j;
  char* cp;
  char* cp2;
  int len;

  cp=(char*)parameter;
  subparam[0]=0;
  for (j=1; j<n; j++)
  {
    if (!strchr(cp,delim))
      return 0;
    cp=strchr(cp,delim)+1;
  }
  cp2=strchr(cp,delim);
  if (cp2)
    len=cp2-cp;
  else
    len=strlen(cp);

  // 3.1.7: size_subparam was not used.
  if (len >= size_subparam)
    return 0;

  strncpy(subparam,cp,len);
  subparam[len]=0;
  cutspaces(subparam);

  // 3.1.7:
  if (!(*subparam))
    return 0;

  return 1;
}

int getsubparam(char*  parameter, 
                int n, 
                char*  subparam,  int size_subparam)
{
  return getsubparam_delim(parameter, n, subparam, size_subparam, ',');
}

int splitline( char*  source, 
              char*  name,  int size_name,
	      char*  value,  int size_value)
{
  char* equalchar;
  int n;

  equalchar=strchr(source,'=');
  value[0]=0;
  name[0]=0;
  if (equalchar)
  {
    strncpy(value,equalchar+1,size_value);
    value[size_value -1]=0;
    cutspaces(value);
    n=equalchar-source;
    if (n>0)
    {
      if (n>size_name-1)
        n=size_name-1;
      strncpy(name,source,n);
      name[n]=0;
      cutspaces(name);
      return 1;
    }
  }
  return 0;
}

int gotosection(FILE* file,  char*  name)
{
  char line[4096 +32];
  char* posi;

  fseek(file,0,SEEK_SET);
  while (fgets(line,sizeof(line),file))
  {
    cutcomment(line);

    if (*line)
    {
      posi=strchr(line,']');
      if ((line[0]=='[') && posi)
      {// 3.1beta7: added brackets, should be a block, otherwise name is still tested.
        *posi=0;
        if (strcmp(line+1,name)==0)
	  return 1;
      }
    }
  }
  return 0;
}

int my_getline(FILE* file,
            char*  name,  int size_name,
	    char*  value,  int size_value)
{
  char line[4096 +32];

  while (fgets(line,sizeof(line),file))
  {
    cutcomment(line);

    // 3.1beta7: lines with one or two illegal characters were not reported:
    //if (Length>2)
    if (*line)
    {
      if (line[0]=='[')
        return 0;

      if (splitline(line,name,size_name,value,size_value)==0)
      {
        strncpy(value,line,size_value);
        value[size_value -1]=0;
        return -1;
      }

      return 1;
    }
  }
  return 0;
}
