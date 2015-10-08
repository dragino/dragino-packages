/* Simple test program
 *
 *  gcc -o test test.c minIni.c
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "minIni.h"

#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))

const char inifile[] = "sip.conf";

int main(void)
{
  char str[100];
  long n;
  int s, k;
  char section[50];

   n = ini_gets("cisco1", "secret", "", str, sizearray(str), inifile);
	
   printf("%s\n", str);	

   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);
   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);
   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);
   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);
   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);
   n = ini_puts("cisco1", "secret", NULL, inifile);
   printf("n=%d\n", n);

  return 0;
}

