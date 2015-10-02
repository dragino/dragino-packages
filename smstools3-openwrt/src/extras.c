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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include "extras.h"
#include "locking.h"
#include "smsd_cfg.h"
#include "logging.h"
#include "alarm.h"

int yesno(char *value)
{
  extern char yes_chars[];
  extern char no_chars[];
  char *p, *p2;

  if (*yes_chars)
  {
    p = yes_chars;
    while (*p)
    {
      if (!(p2 = strchr(p, '\'')))
        break;
      if (!strncmp(value, p, (int)(p2 -p)))
        return 1;
      p = p2 +1;
    }
  }

  if (*no_chars)
  {
    p = no_chars;
    while (*p)
    {
      if (!(p2 = strchr(p, '\'')))
        break;
      if (!strncmp(value, p, (int)(p2 -p)))
        return 0;
      p = p2 +1;
    }
  }

  if ((value[0]=='1') ||
      (value[0]=='y') ||
      (value[0]=='Y') ||
      (value[0]=='t') ||
      (value[0]=='T') ||
      ((value[1]=='n') && (
        (value[0]=='o') ||
        (value[0]=='O'))
      ))
    return 1;
  else
    return 0;
}

int yesno_check(char*  value)
{
  // This version is used to check config file values.
  int result = -1;

  if ((value[0]=='1') ||
      (value[0]=='y') ||
      (value[0]=='Y') ||
      (value[0]=='t') ||
      (value[0]=='T') ||
      ((value[1]=='n') && (
        (value[0]=='o') ||
        (value[0]=='O'))
      ))
    result = 1;
  else
  if ((value[0]=='0') ||
      (value[0]=='n') ||
      (value[0]=='N') ||
      (value[0]=='f') ||
      (value[0]=='F') ||
      ((value[1]=='f') && (
        (value[0]=='o') ||
        (value[0]=='O'))
      ))
    result = 0;

  return result;
}

char *cut_ctrl(char* message) /* removes all ctrl chars */
{
  // 3.0.9: use dynamic buffer to avoid overflow:
  //char tmp[500];
  char *tmp;
  int posdest=0;
  int possource;
  int count;

  count=strlen(message);
  if ((tmp = (char *)malloc(count +1)))
  {
    for (possource=0; possource<=count; possource++)
    {
      // 3.1beta7: added unsigned test:
      if (((unsigned char)message[possource] >= (unsigned char)' ') || (message[possource]==0))
        tmp[posdest++]=message[possource];
    }
    strcpy(message,tmp);
    free(tmp);
  }
  return message;
}

char *cut_crlf(char *st)
{

  while (*st && strchr("\r\n", st[strlen(st) -1]))
    st[strlen(st) -1] = 0;

  return st;
}

int is_blank(char c)
{
  return (c==9) || (c==32);
}

int line_is_blank(char *line)
{
  int i = 0;

  while (line[i])
    if (strchr("\t \r\n", line[i]))
      i++;
    else
      break;

  return(line[i] == 0);
}

int movefile( char*  filename,  char*  directory)
{
  char newname[PATH_MAX];
  char storage[1024];
  int source,dest;
  int readcount;
  char* cp;
  struct stat statbuf;

  if (stat(filename,&statbuf)<0)
    statbuf.st_mode=0640;
  statbuf.st_mode&=07777;
  cp=strrchr(filename,'/');
  if (cp)
    sprintf(newname,"%s%s",directory,cp);
  else
    sprintf(newname,"%s/%s",directory,filename);
  source=open(filename,O_RDONLY);
  if (source>=0)
  {
    dest=open(newname,O_WRONLY|O_CREAT|O_TRUNC,statbuf.st_mode);
    if (dest>=0)
    {
      //while ((readcount=read(source,&storage,sizeof(storage)))>0)
      //  if (write(dest,&storage,readcount)<readcount)
      while ((readcount = read(source, storage, sizeof(storage))) > 0)
        if (write(dest, storage, readcount) < readcount)
	{
	  close(dest);
	  close(source);
	  return 0;
	}
      close(dest);
      close(source);
      unlink(filename);
      return 1;
    }
    else
    {
      close(source);
      return 0;
    }
  }
  else
    return 0;
}

// 3.1beta7: Return values:
// 0 = OK.
// 1 = lockfile cannot be created. It exists.
// 2 = file copying failed.
// 3 = lockfile removing failed.
int movefilewithdestlock_new(char* filename, char* directory, int keep_fname, int store_original_fname, char *prefix, char *newfilename)
{
  if (newfilename)
    *newfilename = 0;

  if (keep_fname)
  {
    char lockfilename[PATH_MAX];
    char* cp;

    //create lockfilename in destination
    cp=strrchr(filename,'/');
    if (cp)
      sprintf(lockfilename,"%s%s",directory,cp);
    else
      sprintf(lockfilename,"%s/%s",directory,filename);
    //create lock and move file
    if (!lockfile(lockfilename))
      return 1;
    if (!movefile(filename,directory))
    {
      unlockfile(lockfilename);
      return 2;
    }
    if (!unlockfile(lockfilename))
      return 3;

    if (newfilename)
      strcpy(newfilename, lockfilename);

    return 0;
  }
  else
  {
    // A new unique name is created to the destination directory.
    char newname[PATH_MAX];
    int result = 0;
    char line[1024];
    int in_headers = 1;
    FILE *fp;
    FILE *fpnew;
    size_t n;
    char *p;
    extern const char *HDR_OriginalFilename;
    extern char HDR_OriginalFilename2[];

    strcpy(line, prefix);
    if (*line)
      strcat(line, ".");
    sprintf(newname,"%s/%sXXXXXX", directory, line);
    close(mkstemp(newname));
    if (!lockfile(newname))
      result = 1;

    unlink(newname);
    if (!result)
    {
      if (!(fpnew = fopen(newname, "w")))
        result = 2;
      else
      {
        if (!(fp = fopen(filename, "r")))
        {
          fclose(fpnew);
          unlink(newname);
          result = 2;
        }
        else
        {
          while (in_headers && fgets(line, sizeof(line), fp))
          {
            if (line_is_blank(line))
            {
              if (store_original_fname && *HDR_OriginalFilename2 != '-')
              {
                p = strrchr(filename, '/');
                fprintf(fpnew, "%s %s\n", (*HDR_OriginalFilename2)? HDR_OriginalFilename2 : HDR_OriginalFilename,
                        (p)? p +1 : filename);
              }
              in_headers = 0;
            }
            fwrite(line, 1, strlen(line), fpnew);
          }

          while ((n = fread(line, 1, sizeof(line), fp)) > 0)
            fwrite(line, 1, n, fpnew);

          fclose(fpnew);
          fclose(fp);
        }
      }
    }

    if (!unlockfile(newname))
    {
      unlink(newname);
      if (!result)
        result = 3;
    }
    else
    {
      unlink(filename);
      if (newfilename)
        strcpy(newfilename, newname);
    }

    return result;
  }
}

char *cutspaces(char *text)
{
  int count;
  int Length;
  int i;
  int omitted;

  /* count ctrl chars and spaces at the beginning */
  count=0;
  while ((text[count]!=0) && ((is_blank(text[count])) || (iscntrl((int)text[count]))) )
    count++;
  /* remove ctrl chars at the beginning and \r within the text */
  omitted=0;
  Length=strlen(text);
  for (i=0; i<=(Length-count); i++)
    if (text[i+count]=='\r')
      omitted++;
    else
      text[i-omitted]=text[i+count];
  Length=strlen(text);
  while ((Length>0) && ((is_blank(text[Length-1])) || (iscntrl((int)text[Length-1]))))
  {
    text[Length-1]=0;
    Length--;
  }

  return text;
}

char *cut_emptylines(char *text)
{
  char* posi;
  char* found;

  posi=text;
  while (posi[0] && (found=strchr(posi,'\n')))
  {
    if ((found[1]=='\n') || (found==text))
      memmove(found,found+1,strlen(found));
    else
      posi++;
  }
  return text;
}

int is_number( char*  text)
{
  int i;
  int Length;

  Length=strlen(text);
  for (i=0; i<Length; i++)
    if (((text[i]>'9') || (text[i]<'0')) && (text[i]!='-'))
      return 0;
  return 1;
}

int is_highpriority(char *filename)
{
  FILE *fp;
  char line[256];
  int result = 0;
  // 3.1beta7: language settings used:
  extern const char *HDR_Priority;
  extern char HDR_Priority2[];
  int hlen;
  int hlen2 = 0;
  char *compare;
  char *compare2 = 0;

  if (ignore_outgoing_priority || spool_directory_order)
    return 0;

  // get_header() and test_header() can be moved to this file,
  // but this is faster:
  if (*HDR_Priority2 && strcmp(HDR_Priority2, "-"))
  {
    if (*HDR_Priority2 == '-')
      compare2 = HDR_Priority2 +1;
    else
      compare2 = HDR_Priority2;
    hlen2 = strlen(compare2);
  }

  compare = (char *)HDR_Priority;
  hlen = strlen(compare);

  if ((fp = fopen(filename, "r")))
  {
    while (!result && fgets(line, sizeof(line), fp))
    {
      if (line_is_blank(line))
        break;

      if ((compare2 && strncmp(line, compare2, hlen2) == 0) ||
          strncmp(line, compare, hlen) == 0)
      {
        cutspaces(strcpyo(line, line + hlen));
        if (!strcasecmp(line,"HIGH"))
          result = 1;
        else if (yesno(line) == 1)
          result = 1;
      }
    }
    fclose(fp);
  }
  return result;
}

int file_is_writable(char *filename)
{
  int result = 0;
  FILE *fp;
  struct stat statbuf;

  // 3.1.12: First check that the file exists:
  if (stat(filename, &statbuf) == 0)
  {
    if (S_ISDIR(statbuf.st_mode) == 0)
    {
      if ((fp = fopen(filename, "a")))
      {
        result = 1;
        fclose(fp);
      }
    }
  }

  return result;
}

int getpdufile(char *filename)
{
  int result = 0;
  struct stat statbuf;
  DIR* dirdata;
  struct dirent* ent;
  char tmpname[PATH_MAX];

  if (*filename)
  {
    if (filename[strlen(filename) -1] != '/')
    {
      if (file_is_writable(filename))
        result = 1;
    }
    else if (!strchr(filename, '.'))
    {
      if (stat(filename, &statbuf) == 0)
      {
        if (S_ISDIR(statbuf.st_mode))
        {
          if ((dirdata = opendir(filename)))
          {
            while ((ent = readdir(dirdata)))
            {
              if (ent->d_name[0] != '.')
              {
                sprintf(tmpname, "%s%s", filename, ent->d_name);
                if (file_is_writable(tmpname))
                {
                  strcpy(filename, tmpname);
                  result = 1;
                  break;
                }
              }
            }
            // 3.1.1:
            //close the directory. added by Callan Fox to fix open handles problem
            closedir(dirdata);
          }
        }
      }
    }
  }

  return result;
}

int getfile(int trust_directory, char *dir, char *filename, int lock)
{
  DIR* dirdata;
  struct dirent* ent;
  struct stat statbuf;
  int found=0;
  time_t mtime;
  char fname[PATH_MAX];
  char tmpname[PATH_MAX];
  int found_highpriority;
  int i;
  char storage_key[PATH_MAX +3];
  unsigned long long start_time;

  // 3.1.12: Collect filenames:
  typedef struct
  {
    char fname[NAME_MAX + 1];
    time_t mtime;
  } _candidate;

  _candidate candidates[NUMBER_OF_MODEMS];
  int lost_count = 0;
  int files_count;
  int locked_count;

#ifdef DEBUGMSG
  printf("!! getfile(dir=%s, ...)\n", dir);
#endif

  start_time = time_usec();

  // 3.1.12: if a file is lost, try a new search immediately.
  while (1)
  {
    if (terminate)
      break;

    // Oldest file is searched. With heavy traffic the first file found is not necesssary the oldest one.

    if (!(dirdata = opendir(dir)))
    {
      // Something has happened to dir after startup check was done successfully.
      writelogfile0(LOG_CRIT, 0, tb_sprintf("Stopping. Cannot open dir %s %s", dir, strerror(errno)));
      alarm_handler0(LOG_CRIT, tb);
      abnormal_termination(1);
    }

    mtime = 0;
    files_count = 0;
    locked_count = 0;
    found_highpriority = 0;
    memset(candidates, 0, sizeof(candidates));

    while ((ent = readdir(dirdata)))
    {
#ifdef DEBUGMSG
      printf("**readdir(): %s\n", ent->d_name);
#endif
      sprintf(tmpname, "%s/%s", dir, ent->d_name);

      // 3.1.12:
      //stat(tmpname, &statbuf);
      if (stat(tmpname, &statbuf) != 0)
        continue;

      if (S_ISDIR(statbuf.st_mode) != 0) /* Is this a directory? */
        continue;

      // 3.1.7:
      //if (strcmp(tmpname + strlen(tmpname) - 5, ".LOCK") != 0)
      i = 1;
      if (strlen(tmpname) >= 5 && !strcmp(tmpname + strlen(tmpname) - 5, ".LOCK"))
        i = 0;
      else if (!strncmp(tmpname, "LOCKED", 6))
        i = 0;

      if (!i)
      {
        locked_count++;
        continue;
      }

      files_count++;

      // 3.1.12:
      //if (trust_directory || !islocked(tmpname)) {...
      if (islocked(tmpname))
        continue;

      sprintf(storage_key, "*%s*\n", tmpname);

      // 3.1beta7, 3.0.10:
      if (os_cygwin)
        if (!check_access(tmpname))
          chmod(tmpname, 0766);

      if (!trust_directory && !os_cygwin && !file_is_writable(tmpname))
      {
        // Try to fix permissions.
        int result = 1;
        char tmp_filename[PATH_MAX +7];
        FILE *fp;
        FILE *fptmp;
        char buffer[1024];
        size_t n;

        snprintf(tmp_filename, sizeof(tmp_filename), "%s.XXXXXX", tmpname);
        close(mkstemp(tmp_filename));
        unlink(tmp_filename);

        if ((fptmp = fopen(tmp_filename, "w")) == NULL)
          result = 0;
        else
        {
          if ((fp = fopen(tmpname, "r")) == NULL)
            result = 0;
          else
          {
            while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
              fwrite(buffer, 1, n, fptmp);

            fclose(fp);
          }

          fclose(fptmp);

          if (result)
          {
            unlink(tmpname);
            rename(tmp_filename, tmpname);
          }
          else
            unlink(tmp_filename);
        }
      }

      if (!trust_directory && !file_is_writable(tmpname))
      {
        int report = 1;
        char reason[100];

        if (!check_access(tmpname))
        {
          snprintf(reason, sizeof(reason), "%s", "Access denied. Check the file and directory permissions.");
          if (getfile_err_store)
            if (strstr(getfile_err_store, storage_key))
              report = 0; 

          if (report)
          {
            strcat_realloc(&getfile_err_store, storage_key, 0);
            writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot handle %s: %s", tmpname, reason));
            alarm_handler0(LOG_ERR, tb);
          }
        }
        else
        {
          // 3.1.5: This error is repeated:
          snprintf(reason, sizeof(reason), "%s", "Dont know why. Check the file and directory permissions.");
          writelogfile0(LOG_ERR, 0, tb_sprintf("Cannot handle %s: %s", tmpname, reason));
          alarm_handler0(LOG_ERR, tb);
        }
      }
      else
      {
        // Forget previous error with this file:
        if (getfile_err_store)
        {
          char *p;
          int l = strlen(storage_key);

          if ((p = strstr(getfile_err_store, storage_key)))
            memmove(p, p +l, strlen(p) -l +1);
          if (!(*getfile_err_store))
          {
            free(getfile_err_store);
            getfile_err_store = NULL;
          }
        }

        i = is_highpriority(tmpname);
        if (found_highpriority && !i)
        {
#ifdef DEBUGMSG
          printf("**%s %s not highpriority, already have one.\n", dir, ent->d_name);
#endif
          continue;
        }

        if (i && !found_highpriority)
        {
          // Forget possible previous found normal priority file:
          mtime = 0;
          found_highpriority = 1;
          memset(candidates, 0, sizeof(candidates));
        }

#ifdef DEBUGMSG
        printf("**%s %s %i ", dir, ent->d_name, (int)(statbuf.st_mtime));
#endif

        // 3.1.6: Files with the same timestamp: compare names:
        if (found && statbuf.st_mtime == mtime)
          if (strcmp(fname, tmpname) > 0)
            mtime = 0;

        if (mtime == 0 || statbuf.st_mtime < mtime)
        {
#ifdef DEBUGMSG
          printf("taken\n");
#endif
          strcpy(fname, tmpname);
          mtime = statbuf.st_mtime;
          found = 1;

          if (spool_directory_order)
            break;

#if NUMBER_OF_MODEMS > 1
          for (i = NUMBER_OF_MODEMS - 1; i > 0; i--)
          {
            strcpy(candidates[i].fname, candidates[i - 1].fname);
            candidates[i].mtime = candidates[i - 1].mtime;
          }
          snprintf(candidates[0].fname, sizeof(candidates[0].fname), "%s", ent->d_name);
          candidates[0].mtime = statbuf.st_mtime;
#endif
        }
        else
        {
#ifdef DEBUGMSG
          printf("leaved\n");
#endif

#if NUMBER_OF_MODEMS > 1
          for (i = 1; i < NUMBER_OF_MODEMS; i++)
          {
            if (candidates[i].fname[0] == 0)
              break;

            if (candidates[i].mtime > statbuf.st_mtime)
              break;

            if (candidates[i].mtime == statbuf.st_mtime)
              if (strcmp(candidates[i].fname, tmpname) > 0)
                break;
          }

          if (i < NUMBER_OF_MODEMS)
          {
            int j;

            for (j = NUMBER_OF_MODEMS - 1; j > i; j--)
            {
              strcpy(candidates[j].fname, candidates[j - 1].fname);
              candidates[j].mtime = candidates[j - 1].mtime;
            }
            snprintf(candidates[i].fname, sizeof(candidates[i].fname), "%s", ent->d_name);
            candidates[i].mtime = statbuf.st_mtime;
          }
#endif
        }
      }
    }

#ifdef DEBUGMSG
    if (getfile_err_store)
      printf("!! process: %i, getfile_err_store:\n%s", process_id, getfile_err_store);
#endif

    // Each process has it's own error storage.
    // Mainspooler handles only the outgoing folder.
    // Modem processes handle all queue directories which are defined to the modem.
    // If some problematic file is deleted (outside of smsd), it's name remains in the storage.
    // To avoid missing error messages with the same filename later, storage is checked and cleaned.

    if (getfile_err_store)
    {
      char *p1;
      char *p2;
      char tmp[PATH_MAX];
      struct stat statbuf;

      p1 = getfile_err_store;
      while ((p2 = strchr(p1, '\n')))
      {
        memcpy(tmp, p1 +1, p2 -p1 -2);
        tmp[p2 -p1 -2] = 0;
        //if (access(tmp, F_OK) != 0)
        if (stat(tmp, &statbuf))
          memmove(p1, p2 +1, strlen(p2));
        else
          p1 = p2 +1;
      }

      if (!(*getfile_err_store))
      {
        free(getfile_err_store);
        getfile_err_store = NULL;
      }
    }

#ifdef DEBUGMSG
    if (getfile_err_store)
      printf("!! process: %i, getfile_err_store:\n%s", process_id, getfile_err_store);
#endif

    // 3.1.9: Lock the file before waiting.
    if (found && lock)
    {
      // 3.1.12: check if a file still exists:
      if (stat(fname, &statbuf) || !lockfile(fname))
      {
        found = 0;

#if NUMBER_OF_MODEMS > 1
        // Try to take the next best file:
        for (i = 1; i < NUMBER_OF_MODEMS && candidates[i].fname[0] && !found; i++)
        {
          sprintf(fname, "%s/%s", dir, candidates[i].fname);
          if (stat(fname, &statbuf) == 0 && lockfile(fname))
          {
            mtime = candidates[i].mtime;
            found = 1;
            //writelogfile(LOG_DEBUG, 0, "Got the next best file from candidates (%i), %i SMS files and %i LOCK files seen.", i, files_count, locked_count);
          }
        }
#endif

        if (found == 0)
        {
          lost_count++;

          // 3.1.12: continue immediately, or do other tasks after trying enough
          if (max_continuous_sending == 0 || time_usec() < start_time + max_continuous_sending * 1000000)
          {
            closedir(dirdata);
            continue;
          }
          else if (max_continuous_sending > 0)
            writelogfile(LOG_DEBUG, 0, "Tried to get a file for %i seconds, will do other tasks and then continue.", (int)(time_usec() - start_time) / 1000000);
        }
      }
    }

    if (!trust_directory && found)
    {
      /* check if the file grows at the moment (another program writes to it) */
      int groesse1;
      int groesse2;

      // 3.1.9: check if the file is deleted, or deleted while waiting...
      if (stat(fname, &statbuf))
        found = 0;
      else
      {
        groesse1 = statbuf.st_size;

        // 3.1.12: sleep less:
        //sleep(1);
        usleep_until(time_usec() + 500000);
            
        if (stat(fname, &statbuf))
          groesse2 = -1;
        else
          groesse2 = statbuf.st_size;
        if (groesse1 != groesse2)
          found = 0;
      }

      if (!found && lock)
        unlockfile(fname);
    }

    closedir(dirdata);

    break;
  }

  if (!found)
    *filename = 0;
  else
  {
    strcpy(filename, fname);

    // 3.1.12:
    i = (int)(time_usec() - start_time) / 100000;
    if (i > 10)
      writelogfile((i >= 50)? LOG_NOTICE : LOG_DEBUG, 0, "Took %.1f seconds to get a file %s, lost %i times, %i SMS files and %i LOCK files seen.", (double)i / 10, fname, lost_count, files_count, locked_count);
  }

#ifdef DEBUGMSG
  printf("## result for dir %s: %s\n\n", dir, filename);
#endif
  return found;
}

int my_system(
	//
	// Executes an external process.
	//
	char *command,
	char *info
)
{
	int pid;
	int status;
	time_t start_time;
	char *p;
	char tmp1[PATH_MAX];
	char tmp2[PATH_MAX];

	// Cannot contain "(" when passed as an argument
	snprintf(tmp1, sizeof(tmp1), ">%s/smsd_%s_1.XXXXXX", "/tmp", info);
	while ((p = strchr(tmp1, '(')))
		*p = '.';
	while ((p = strchr(tmp1, ')')))
		*p = '.';
	while ((p = strchr(tmp1, ' ')))
		*p = '-';

	snprintf(tmp2, sizeof(tmp2), "2>%s/smsd_%s_2.XXXXXX", "/tmp", info);
	while ((p = strchr(tmp2, '(')))
		*p = '.';
	while ((p = strchr(tmp2, ')')))
		*p = '.';
	while ((p = strchr(tmp2, ' ')))
		*p = '-';

	if (!ignore_exec_output) // 3.1.7
	{
		close(mkstemp(tmp1 + 1));
		close(mkstemp(tmp2 + 2));
	}

	start_time = time(0);
#ifdef DEBUGMSG
	printf("!! my_system(%s, %s)\n", command, info);
#endif
	writelogfile0(LOG_DEBUG, 0, tb_sprintf("Running %s: %s", info, command));

	pid = fork();
	if (pid == -1)
	{
		// 3.1.12:
		//writelogfile0(LOG_CRIT, 0, tb_sprintf("Fatal error: fork failed."));
		writelogfile0(LOG_CRIT, 0, tb_sprintf("Fatal error: fork failed. %i, %s", errno, strerror(errno)));

		return -1;
	}

	if (pid == 0)		// only executed in the child
	{
		char *argv[4];
		char *cmd = 0;

#ifdef DEBUGMSG
		printf("!! pid=%i, child running external command\n", pid);
#endif

		// TODO: sh still ok?
		argv[0] = "sh";
		if ((p = strrchr(shell, '/')))
			argv[0] = p + 1;

		argv[1] = "-c";
		argv[2] = command;	//(char*) command;

		if (!ignore_exec_output)
		{
			if ((cmd = (char *) malloc(strlen(command) + strlen(tmp1) + strlen(tmp2) + 3)))
			{
				sprintf(cmd, "%s %s %s", command, tmp1, tmp2);
				argv[2] = cmd;
			}
		}

		argv[3] = 0;

		// 3.1.5:
		//execv("/bin/sh",argv);  // replace child with the external command
		execv(shell, argv);	// replace child with the external command
		writelogfile0(LOG_CRIT, 1, tb_sprintf("Fatal error: execv( %s ) returned: %i, %s", shell, errno, strerror(errno)));

		free(cmd);

#ifdef DEBUGMSG
		printf("!! pid=%i, execv() failed, %i, %s\n", pid, errno, strerror(errno));
		printf("!! child exits now\n");
#endif
		exit((errno) ? errno : 1);	// exit with error when the execv call failed
	}

	errno = 0;
#ifdef DEBUGMSG
	printf("!! father waiting for child %i\n", pid);
#endif
	snprintf(run_info, sizeof(run_info), "%s", info);

	while (1)
	{
		if (waitpid(pid, &status, 0) == -1)
		{
			if (errno != EINTR)
			{
				*run_info = 0;
				writelogfile0(LOG_ERR, 0, tb_sprintf("Done: %s, execution time %i sec., errno: %i, %s", info, time(0) - start_time, errno, strerror(errno)));
				return -1;
			}
		}
		else
		{
			int level = LOG_DEBUG;
			int trouble = 0;

			*run_info = 0;

			// 3.1.6: When running checkhandler and it spooled a message, return value 2 SHOULD NOT activate trouble logging:
			//writelogfile0((status == 0) ? LOG_DEBUG : LOG_ERR, (status == 0) ? 0 : 1, tb_sprintf("Done: %s, execution time %i sec., status: %i", info, time(0) - start_time, status));
			if (!strcmp(info, "checkhandler"))
			{
				if (status != 0 && WEXITSTATUS(status) != 2)
				{
					level = LOG_ERR;
					trouble = 1;
				}
			}
			else if (status != 0)
			{
				level = LOG_ERR;
				trouble = 1;
			}

			writelogfile0(level, trouble, tb_sprintf("Done: %s, execution time %i sec., status: %i (%i)", info, time(0) - start_time, status, WEXITSTATUS(status)));

			if (!ignore_exec_output)
			{
				struct stat statbuf;
				FILE *fp;
				char line[2048];
				int i;
				char *p;

				for (i = 1; i <= 2; i++)
				{
					p = (i == 1) ? tmp1 + 1 : tmp2 + 2;

					if (stat(p, &statbuf) == 0)
					{
						if (statbuf.st_size)
						{
							writelogfile0(LOG_ERR, 1, tb_sprintf("Exec: %s %s:", info, (i == 1) ? "said something" : "encountered errors"));
							if ((fp = fopen(p, "r")))
							{
								while (fgets(line, sizeof(line), fp))
								{
									while (strlen(line) > 1 && strchr("\r\n", line[strlen(line) - 1]))
										line[strlen(line) - 1] = 0;

									writelogfile0(LOG_ERR, 1, tb_sprintf("! %s", line));
								}
								fclose(fp);
							}
						}

						unlink(p);
					}
				}
			}

			return WEXITSTATUS(status);
		}
	}
}

int write_pid( char* filename)
{
  char pid[20];
  int pidfile;

  sprintf(pid,"%i\n", (int)getpid());
  pidfile = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0644);
  if (pidfile >= 0)
  {
    write(pidfile, pid, strlen(pid));
    close(pidfile);
    return 1;
  }
  return 0;
}

int check_pid(char *filename)
{
  int result = 0;
  char pid[20];
  FILE *fp;
  char buffer[256];

  sprintf(pid,"%i\n", (int)getpid());
  if ((fp = fopen(filename, "r")))
  {
    if (fgets(buffer, sizeof(buffer), fp))
      if (strcmp(pid, buffer))
        result = atoi(buffer);

    fclose(fp);
  }

  return result;
}

void remove_pid( char* filename)
{
  if (*filename)
    unlink(filename);
}

int parse_validity(char *value, int defaultvalue)
{
  int result = defaultvalue;
  char buffer[100];
  int i;
  char tmp[100];
  int got_numbers = 0;
  int got_letters = 0;
  int idx;
  char *p;

  if (value && *value)
  {
    // n min, hour, day, week, month, year
    // 3.0.9: if only keyword is given, insert number 1.
    // Fixed number without keyword handling.
    // Convert to lowercase so upcase is also accepted.

    *buffer = 0;
    snprintf(tmp, sizeof(tmp), "%s", value);
    cutspaces(tmp);
    for (idx = 0; tmp[idx]; idx++)
    {
      tmp[idx] = tolower((int)tmp[idx]);
      if (tmp[idx] == '\t')
        tmp[idx] = ' ';
      if (isdigitc(tmp[idx]))
        got_numbers = 1;
      else
        got_letters = 1;
    }

    if (got_numbers && !got_letters)
    {
      i = atoi(tmp);
      if (i >= 0 && i <= 255)
        result = i;
      return result;
    }

    if ((p = strchr(tmp, ' ')))
      *p = 0;

    if (strstr("min hour day week month year", tmp))
      sprintf(buffer, "1 %.*s", (int)sizeof(buffer) -3, tmp);
    else
      sprintf(buffer, "%.*s", (int)sizeof(buffer) -1, value);

    while ((i = atoi(buffer)) > 0)
    {
      // 0 ... 143     (value + 1) * 5 minutes (i.e. 5 minutes intervals up to 12 hours)
      if (strstr(buffer, "min"))
      {
        if (i <= 720)
        {
          result = (i < 5)? 0 : i /5 -1;
          break;
        }
        sprintf(buffer, "%i hour", i /= 60);
      }

      // 144 ... 167   12 hours + ((value - 143) * 30 minutes) (i.e. 30 min intervals up to 24 hours)
      if (strstr(buffer, "hour"))
      {
        if (i <= 12)
        {
          sprintf(buffer, "%i min", i *60);
          continue;
        }
        if (i <= 24)
        {
          result = (i -12) *2 +143;
          break;
        }
        sprintf(buffer, "%i day", i /= 24);
      }

      // 168 ... 196   (value - 166) * 1 day (i.e. 1 day intervals up to 30 days)
      if (strstr(buffer, "day"))
      {
        if (i < 2)
        {
          sprintf(buffer, "24 hour");
          continue;
        }
        if (i <= 34)
        {
          result = (i <= 30)? i +166 : 30 +166;
          break;
        }
        sprintf(buffer, "%i week", i /= 7);
      }

      // 197 ... 255   (value - 192) * 1 week (i.e. 1 week intervals up to 63 weeks)
      if (strstr(buffer, "week"))
      {
        if (i < 5)
        {
          sprintf(buffer, "%i day", i *7);
          continue;
        }
        result = (i <= 63)? i +192 : 255;
        break;
      }

      if (strstr(buffer, "month"))
      {
        sprintf(buffer, "%i day", (i == 12)? 365 : i *30);
        continue;
      }

      if (strstr(buffer, "year"))
      {
        if (i == 1)
        {
          sprintf(buffer, "52 week");
          continue;
        }
        result = 255;
      }

      break;
    }
  }

  return result;
}

// 0=invalid, 1=valid
int report_validity(char *buffer, int validity_period)
{
  int result = 0;
  int n;
  char *p;

  if (validity_period < 0 || validity_period > 255)
    sprintf(buffer, "invalid (%i)", validity_period);
  else
  {
    if (validity_period <= 143)
    {
      // 0 ... 143    (value + 1) * 5 minutes (i.e. 5 minutes intervals up to 12 hours)
      n = (validity_period +1) *5;
      p = "min";
    }
    else if (validity_period <= 167)
    {
      // 144 ... 167  12 hours + ((value - 143) * 30 minutes) (i.e. 30 min intervals up to 24 hours)
      n =  12 +(validity_period -143) /2;
      p = "hour";
    }
    else if (validity_period <= 196)
    {
      // 168 ... 196  (value - 166) * 1 day (i.e. 1 day intervals up to 30 days)
      n = validity_period -166;
      p = "day";
    }
    else
    {
      // 197 ... 255  (value - 192) * 1 week (i.e. 1 week intervals up to 63 weeks)
      n = validity_period -192;
      p = "week";
    }

    sprintf(buffer, "%i %s%s (%i)", n, p, (n > 1)? "s" : "", validity_period);
    result = 1;
  }

  return result;
}

int getrand(int toprange)
{
  srand((int)(time(NULL) * getpid()));
  return (rand() % toprange) +1;
}

int is_executable(char *filename)
{
  // access() migth do this easier, but in Gygwin it returns 0 even when requested permissions are NOT granted.
  int result = 0;
  struct stat statbuf;
  mode_t mode;
  int n, i;
  gid_t *g;

  if (stat(filename, &statbuf) >= 0)
  {
    mode = statbuf.st_mode & 0755;

    if (getuid())
    {
      if (statbuf.st_uid != getuid())
      {
        if ((n = getgroups(0, NULL)) > 0)
        {
          if ((g = (gid_t *)malloc(n * sizeof(gid_t))))
          {
            if ((n = getgroups(n, g)) > 0)
            {
              for (i = 0; (i < n) & (!result); i++)
                if (g[i] == statbuf.st_gid)
                  result = 1;
            }
            free(g);
          }
        }

        if (result)
        {
          if ((mode & 050) != 050)
            result = 0;
        }
        else if ((mode & 05) == 05)
          result = 1;
      }
      else if ((mode & 0500) == 0500)
        result = 1;
    }
    else if ((mode & 0100) || (mode & 010) || (mode & 01))
      result = 1;
  }

  return result;
}

int check_access(char *filename)
{
  // access() migth do this easier, but in Gygwin it returns 0 even when requested permissions are NOT granted.
  int result = 0;
  struct stat statbuf;
  mode_t mode;
  int n, i;
  gid_t *g;

  if (stat(filename, &statbuf) >= 0)
  {
    mode = statbuf.st_mode; // & 0777;

    if (getuid())
    {
      if (statbuf.st_uid != getuid())
      {
        if ((n = getgroups(0, NULL)) > 0)
        {
          if ((g = (gid_t *)malloc(n * sizeof(gid_t))))
          {
            if ((n = getgroups(n, g)) > 0)
            {
              for (i = 0; (i < n) & (!result); i++)
                if (g[i] == statbuf.st_gid)
                  result = 1;
            }
            free(g);
          }
        }

        if (result)
        {
          if ((mode & 060) != 060)
            result = 0;
        }
        else if ((mode & 06) == 06)
          result = 1;
      }
      else if ((mode & 0600) == 0600)
        result = 1;
    }
    else if ((mode & 0200) || (mode & 020) || (mode & 02))
      result = 1;
  }

  return result;
}

int value_in(int value, int arg_count, ...)
{
  int result = 0;
  va_list ap;

  va_start(ap, arg_count);
  for (; arg_count > 0; arg_count--)
    if (value == va_arg(ap, int))
      result = 1;

  va_end(ap);

  return result;
}

int t_sleep(int seconds)
{
  // 3.1.12: When a signal handler is installed, receiving of any singal causes
  // that functions sleep() and usleep() will return immediately.
  //int i;
  time_t t;

  t = time(0);
  //for (i = 0; i < seconds; i++)
  while (time(0) - t < seconds)
  {
    if (terminate)
      return 1;

    sleep(1);
  }

  return 0;
}

int usleep_until(unsigned long long target_time)
{
  struct timeval tv;
  struct timezone tz;
  unsigned long long now;

  do
  {
    gettimeofday(&tv, &tz);
    now = (unsigned long long)tv.tv_sec *1000000 +tv.tv_usec;

    if (terminate == 1)
      return 1;

    if (now < target_time)
      usleep(100);
  }
  while (now < target_time);

  return 0;
}

unsigned long long time_usec()
{
  struct timeval tv;
  struct timezone tz;
  //struct tm *tm;

  gettimeofday(&tv, &tz);
  /*tm =*/ //localtime(&tv.tv_sec);

  return (unsigned long long)tv.tv_sec *1000000 +tv.tv_usec;
}

int make_datetime_string(char *dest, size_t dest_size, char *a_date, char *a_time, char *a_format)
{
  int result = 0;
  time_t rawtime;
  struct tm *timeinfo;

  //time(&rawtime);

  // 3.1.14:
  //if (!a_date && !a_time)
  //  return strftime(dest, dest_size, (a_format)? a_format : datetime_format, localtime(&rawtime));
  if (!a_date && !a_time)
  {
    struct timeval tv;
    struct timezone tz;
    char *p;
    char buffer[7];

    gettimeofday(&tv, &tz);
    rawtime = tv.tv_sec;
    timeinfo = localtime(&rawtime);
    result = strftime(dest, dest_size, (a_format)? a_format : datetime_format, timeinfo);

    if ((p = strstr(dest, "timeus")))
    {
      snprintf(buffer, sizeof(buffer), "%06d", (int)tv.tv_usec);
      strncpy(p, buffer, strlen(buffer));
    }
    else if ((p = strstr(dest, "timems")))
    {
      snprintf(buffer, sizeof(buffer), "%03d", (int)tv.tv_usec / 1000);
      strncpy(p, buffer, strlen(buffer));
      memmove(p + 3, p + 6, strlen(p + 6) + 1);
    }

    return result;
  }

  if (a_date && strlen(a_date) >= 8 && a_time && strlen(a_time) >= 8)
  {
    time(&rawtime);

    timeinfo = localtime(&rawtime);
    timeinfo->tm_year = atoi(a_date) + 100;
    timeinfo->tm_mon = atoi(a_date + 3) - 1;
    timeinfo->tm_mday = atoi(a_date + 6);
    timeinfo->tm_hour = atoi(a_time);
    timeinfo->tm_min = atoi(a_time + 3);
    timeinfo->tm_sec = atoi(a_time + 6);
    // ?? mktime(timeinfo);
    result = strftime(dest, dest_size, (a_format)? a_format : datetime_format, timeinfo);
  }

  return result;
}

void strcat_realloc(char **buffer, char *str, char *delimiter)
{
  int delimiter_length = 0;

  if (delimiter)
    delimiter_length = strlen(delimiter);

  if (*buffer == 0)
  {
    if ((*buffer = (char *) malloc(strlen(str) + delimiter_length + 1)))
      **buffer = 0;
  }
  else
    *buffer = (char *) realloc((void *) *buffer, strlen(*buffer) + strlen(str) + delimiter_length + 1);

  if (*buffer)
    sprintf(strchr(*buffer, 0), "%s%s", str, (delimiter) ? delimiter : "");
}

char *strcpyo(char *dest, const char *src)
{
  size_t i;

  for (i = 0; src[i] != '\0'; i++)
    dest[i] = src[i];

  dest[i] = '\0';

  return dest;
}

void getfield(char* line, int field, char* result, int size)
{
  char* start;
  char* end;
  int i;
  int length;

#ifdef DEBUGMSG
  printf("!! getfield(line=%s, field=%i, ...)\n",line,field);
#endif
  if (size < 1)
    return;

  *result=0;
  start=strstr(line,":");
  if (start==0)
    return;
  for (i=1; i<field; i++)
  {
    start=strchr(start+1,',');
    if (start==0)
      return;      
  }
  start++;
  while (start[0]=='\"' || start[0]==' ')
    start++;
  if (start[0]==0)
    return;
  end=strstr(start,",");
  if (end==0)
    end=start+strlen(start)-1;
  while ((end[0]=='\"' || end[0]=='\"' || end[0]==',') && (end>=start))
    end--;
  length=end-start+1;
  if (length >= size)
    return;
  strncpy(result,start,length);
  result[length]=0;
#ifdef DEBUGMSG
  printf("!! result=%s\n",result);
#endif    
}
