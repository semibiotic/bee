#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#include <bee.h>
#include <db.h>
#include <res.h>
#include <links.h>
#include <logidx.h>

#include "beecutlog.h"

#error FIXME: Program not finished !

#ifndef MAXRECS
#define MAXRECS 32768
#endif

#define ONE_DAY (3600*24)

int         off_flags = 0;

long long   first = 0;
long long   last  = 0;

char      * logname = NULL;
char      * idxname = NULL;

logrec_t    logrec;
logrec_t    prnrec;

char       buf[256];
char       titlebuf[256];

idxhead_t  idxhead;
long long  idxstart = 0;
long long  idxstop  = 0;

int main(int argc, char ** argv)
{  long long    rc;
   tformat_t    tform;

   long long    i;
   int          n;

   int          fd     = (-1);
   int          fd_in  = (-1);
   int          fd_out = (-1);

   int          days = 0;
   struct tm    stmf;
   struct tm    stmt;

// Initialize table format
   memset(&tform, 0, sizeof(tform));

#define PARAMS "F:n:T:"

   while ((rc = getopt(argc, argv, PARAMS)) != -1)
   {
      switch (rc)
      {
         case 'F':
            tform.from = parse_time(optarg);
            break;

         case 'T':
            tform.to = parse_time(optarg);
            break;

         case 'n':
            days = strtol(optarg, NULL, 10);
            break;

         default:
            usage();
            exit(-1); 
      }
   }

// process -n switch (no of days)
   if (days > 0)
   {  if (tform.from == 0 || tform.to != 0)
      {  syslog(LOG_ERR, "-n switch requires -F, but no -T");
         exit(-1);
      }
      tform.to = tform.from + days * ONE_DAY;

// check & correct time if DST changed 
      localtime_r(&(tform.from), &stmf);
      localtime_r(&(tform.to),   &stmt);

      if (stmf.tm_hour != stmt.tm_hour ||
          stmf.tm_min  != stmt.tm_min  ||
          stmf.tm_sec  != stmt.tm_sec)
      {  n = (stmf.tm_hour * 3600 + stmf.tm_min * 60 + stmf.tm_sec) - 
             (stmt.tm_hour * 3600 + stmt.tm_min * 60 + stmt.tm_sec);
         if (abs(n) > (18*3600)) n -= (24*3600) * (n < 0 ? -1 : 1);

         tform.to += n; 
      } 
   }

// Count first/last record with idx file
      while (1) // fictive cycle
      {  fd = open(idxname, O_RDONLY | O_SHLOCK, 0644);
         if (fd < 0)
         {  syslog(LOG_ERR, "open(%s): %m", idxname);
            break;  
         }
         rc = read(fd, &idxhead, sizeof(idxhead));
         if (rc < sizeof(idxhead))
         {  syslog(LOG_ERR, "read(idxhead): %m");
            break;
         }
         // check file marker
         if (memcmp(idxhead.marker, IDXMARKER, sizeof(idxhead.marker)) != 0) break;
         // read start index
         i = ((tform.from - idxhead.first) / ONE_DAY - 2) * sizeof(long long) +
             sizeof(idxhead);
         if (i > 0)
         {  rc = lseek(fd, i, SEEK_SET);
            if (rc == i)
            {  rc = read(fd, &idxstart, sizeof(idxstart));
               if (rc != sizeof(idxstart))  idxstart = 0; 
            }
         }
         // read stop index
         i = ((tform.to - idxhead.first) / ONE_DAY + 1) * sizeof(long long) +
             sizeof(idxhead);
         if (i > 0)
         {  rc = lseek(fd, i, SEEK_SET);
            if (rc == i)
            {  rc = read(fd, &idxstop, sizeof(idxstop));
               if (rc != sizeof(idxstop))  idxstop = 0; 
            }
         }
         
         close(fd); fd = (-1); 
         break;
      }

// Open input log file (w/ lock)
   fd_in = open(logname, O_RDONLY | O_EXLOCK, 0777);
   if (fd_in < 0)
   {  syslog(LOG_ERR, "open(%s): %m", logname);
      return (-1);
   }
   
// Seek to first record
   rc = lseek(fd_in, idxstart * sizeof(logrec_t), SEEK_SET);
   if (rc != idxstart * sizeof(logrec_t))
   {  syslog(LOG_ERR, "lseek(): error (%m)");
      return (-1);
   }

// Rename input


// Open output log file (w/ lock)
   fd_out = open(logname, O_WRONLY | O_CREAT | O_EXCL | O_EXLOCK, 0777);
   if (fd_out < 0)
   {  syslog(LOG_ERR, "open(%s): %m", logname);
      return (-1);
   }

// Copy file tail

// Close files

   return 0;
}


// 1.01.2002.20.53.42

char   delim[]=" \n\t:./\\;,'\"`-";

char   datebuf[128]="";

time_t  parse_time(char * strdate)
{  char      * ptr = datebuf;
   char      * str;
   struct tm   stm;
   time_t      rval;
   int         temp;
   int         eol = 0;

   strlcpy(datebuf, strdate, sizeof(datebuf));

   memset(&stm, 0, sizeof(stm));

// month day 
   str = next_token(&ptr, delim);
   if (str == NULL) return 0;
   stm.tm_mday = strtol(str, NULL, 10);
   if (stm.tm_mday > 1000000000)
   {  str = next_token(&ptr, delim);
      if (str != NULL) return 0;      
      return stm.tm_mday;    // return raw UTC
   }
// month
   str = next_token(&ptr, delim);
   if (str == NULL) return 0;
   stm.tm_mon = strtol(str, NULL, 10) - 1;
// year
   str = next_token(&ptr, delim);
   if (str == NULL) return 0;
   temp = strtol(str, NULL, 10);
   if (temp < 100) temp += 100;
   else temp -= 1900;
   if (temp < 0) return 0;
   stm.tm_year = temp;
// hour (optional)
   str = next_token(&ptr, delim);
   if (str == NULL) eol = 1;
   else stm.tm_hour = strtol(str, NULL, 10);
// minute (optional)
   if (! eol)      
   {  str = next_token(&ptr, delim);
      if (str == NULL) eol = 1;
      else stm.tm_min = strtol(str, NULL, 10);
   }
// seconds (optional)
   if (! eol)      
   {  str = next_token(&ptr, delim);
      if (str == NULL) eol = 1;
      else stm.tm_sec = strtol(str, NULL, 10);
   }
// assemble time_t
   stm.tm_isdst = -1;
   rval = mktime(&stm);
   if (rval < 0) return 0;

   return rval;
}

char * strtime(time_t utc)
{ 
   struct tm     stm;
   static char   tbuf[64];
 
   if (localtime_r(&utc, &stm) == NULL) return "не определено";

   snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%04d %02d:%02d:%02d",
            stm.tm_mday, stm.tm_mon+1, stm.tm_year+1900,
            stm.tm_hour, stm.tm_min, stm.tm_sec);

   return tbuf;
}

void usage()
{
   fprintf(stderr,
"BEE - Small billing solutions project ver. 0.1 \n"
"   Report generator utility\n\n"
"BSD licensed, see LICENSE for details. OpenBSD.RU project\n\n"
" Usage:\n"
"   beecutlog [options] logfile \n"
"      Available options:\n"
"F D:M:Y[h:m[:s]] - from given time (default - most early)\n"
"T D:M:Y[h:m[:s]] - to given time (default - most late)\n"
"F UTCseconds     - from given time (seconds since Epoch)\n"
"T UTCseconds     - to given time (seconds since Epoch)\n"
"n N              - given number of days (requires -F, can't work with -T)\n"
"\n"
);

}
