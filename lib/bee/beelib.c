/* $RuOBSD: beelib.c,v 1.3 2007/09/15 11:03:31 shadow Exp $ */
#include <string.h>
#include <stdlib.h>

#include <bee.h>

char * next_token (char ** ptr, char * delim)
{  char * str;

   do
   {  str = strsep(ptr, delim);
      if (str == NULL) break;
   } while (*str == '\0');

   return str;
}

char * strtrim(char * ptr, char * delim)
{  char * str = ptr;
   int    p;

   if (ptr == NULL) return NULL;

// cut first delimiters
   while (*str != '\0')
   {  if (strchr(delim, *str) != NULL) str++;
      else break;
   }

// cut last delimiters
   p = strlen(str) - 1;
   while (p > 0)
   {  if (strchr(delim, str[p]) != NULL) str[p--] = '\0';
      else break;
   }

   return str;
}
