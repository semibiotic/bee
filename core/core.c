/* $RuOBSD: core.c,v 1.43 2010/11/13 22:18:39 shadow Exp $ */

#include <sys/cdefs.h>
#include <syslog.h>
#include <varargs.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <version.h>
#include <bee.h>
#include <ipc.h>
#include <db.h>
#include <db2.h>
#include <core.h>
#include <command.h>
#include <res.h>
#include <links.h>
#include <timer.h>
#include <tariffs.h>

#include "sqlcfg.h"
#include "queries.h"

/*
   Init billing:
   >> Parse command line
   >> Configuring (if any)
   >> Open Account DB
   >> Open LogDB
   >> Check Accounts & enable resources
   >> Open IPC channel
   >> Hear for modules
   >> Process Queries
 */

accbase_t Accbase;
logbase_t Logbase;

config_t   config = {NULL, NULL, NULL, NULL, NULL, NULL};  // SQL config portion

// DB runtime context
void    * DBdata;

// Session data
int        HumanRead        = 1;
int        MachineRead      = 1;
int        QuietMode        = 0;
int        ResOn            = 0;
char       SessionLogin[32] = "";
long long  SessionPerm      = PERM_SUPERUSER;  // hack
long long  SessionId        = 0;
long long  UserId           = 0;
// Session application data
long long  SessionLastAcc   = 0;

char       * proc_title = NULL;
int          ForceService = (-1);

char       * QueryFilename = NULL;
char       * QueryBuffer   = NULL;

int       NeedUpdate   = 0;

time_t    HackTime = 0;

char      sbuf[128];
char      outbuf[256];

link_t    internal_link = {0,0,0};
link_t  * ld            = &internal_link;

char    * Config_path = NULL; // force default

char    * accbase_name_old = "/var/bee/account.dat";


int main(int argc, char ** argv)
{  int             c;
   int             fd;
   int             rc;
   int             i;
   int             fRun     = 0;
   int             fDaemon  = 0;
   int             fUpdate  = 0;
   int             fConvert = 0;
   int             fSQL     = 0;
   int             fDumpGates = 0;

   accbase_t       Accbase_temp;
   acc_t           new_acc;
   acc_t_old       old_acc; 

   char         ** row;

   res_coreinit();

   openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

//   DumpQuery = 1;

/*
 A  1. Redefine service port
 u  2. Update access at start
 d  3. Run Daemon
 c  4. Run console
*/

   while ((c = getopt(argc, argv, OPTS)) != -1)
   {  switch (c)
      {
         case 'A':
            ForceService = strtol(optarg, NULL, 10);
            break;

         case '2':
            fSQL = 1;
            break;

         case 'v':   // verbose - show queries
            DumpQuery = 1;
            break;


         case 'f':
            Config_path = optarg;
            break;

         case 'c':
            ld->fStdio = 1;
            fRun       = 1;
            break;

         case 'd':
            fDaemon = 1;
            fRun    = 1;
            break;

         case 'u':
            fUpdate = 1;
            break;

         case 'o':
            fConvert = 1;
            break;

         case 't':
            proc_title = optarg;
            break;

         case 'Q':
            QueryFilename = optarg;
            break;

         case 'g':
            fDumpGates = 1;
            break;

         case 'q':
            QuietMode = 1;
            break;

         case 'h':
         case '?':
	    usage(0);

         default:
	    usage(-1);
      }
   }

// Load configuration
   if (QuietMode == 0) fprintf(stderr, "Loading base configuration ... ");

   rc = conf_load(Config_path);
   if (rc < 0)
   {  fprintf(stderr, "FAILURE\n");
      syslog(LOG_ERR, "FAILURE - Can't load configuration");
      exit(-1);
   }
   if (QuietMode == 0) fprintf(stderr, "OK.\n");

   if (fSQL)
   {
      if (QuietMode == 0) fprintf(stderr, "Loading SQL configuration ... ");
      rc = cfg_load(Config_path, &config);
      if (rc < 0)
      {  fprintf(stderr, "ERROR (Fatal, see syslog).\n");
         exit(-1);
      }

      if (QuietMode == 0) fprintf(stderr, "OK.\nLoading scripts ... ");

      rc = qlist_load(config.DBscripts);
      if (rc < 0)
      {  fprintf(stderr, "ERROR (fatal)\n");
         exit(-1);
      }
      if (QuietMode == 0) fprintf(stderr, "OK.\n");

   } // if fSQL

// Converting old account table
   if (fConvert != 0)
   {  fprintf(stderr, "Converting database ... ");

// Open old account file
      rc = acc_baseopen(&Accbase_temp, accbase_name_old);
      if (rc != SUCCESS)
      {  fprintf(stderr, "FAILURE - Can't open old account table\n");
         exit(-1);
      } 

// Open new account file
      rc = acc_baseopen(&Accbase, conf_accfile);
      if (rc != SUCCESS)
      {  rc = open(conf_accfile, O_RDONLY | O_CREAT | O_EXCL, 0600);
         if (rc < 0)
         {  fprintf(stderr, "FAILURE - Can't open/create new account table\n");
            exit(-1);
         }
         close(rc);
         rc = acc_baseopen(&Accbase, conf_accfile);
         if (rc != SUCCESS)
         {  fprintf(stderr, "FAILURE - Can't open created account table\n");
            exit(-1);
         }
      }

// Ensure, that new table is empty
      rc = acc_reccount(&Accbase);
      if (rc != 0)
      {  fprintf(stderr, "FAILURE - New account table must be empty !\n");
         exit(-1);
      }

// Copy accounts data
      memset(&new_acc, 0, sizeof(new_acc));
      for(i=0; 1; i++)
      {  rc = acci_get_old(&Accbase_temp, i, &old_acc);
         if (rc == IO_ERROR)
         {  fprintf(stderr, "FAILURE - I/O error on read, (%d accounts copied) !\n", i);
            exit(-1);
         }
         if (rc == NOT_FOUND) break;
         new_acc.tag     = old_acc.tag;
         new_acc.accno   = old_acc.accno;
         new_acc.balance = old_acc.balance;
         new_acc.start   = old_acc.start;
         new_acc.stop    = old_acc.stop;
         new_acc.tariff  = old_acc.reserv[0];

         rc = acc_add(&Accbase, &new_acc);
         if (rc == IO_ERROR)
         {  fprintf(stderr, "FAILURE - I/O error on write, (%d accounts copied) !\n", i);
            exit(-1);
         }

         if (rc != i)
         {  fprintf(stderr, "FAILURE - Accno sync error (%d accounts copied) !\n", i);
            exit(-1);
         }
      }
      acc_baseclose(&Accbase_temp);
      acc_baseclose(&Accbase);
      fprintf(stderr, "SUCCESS (%d accounts copied) !\n", i);
      exit(-1);
   }

   if (fDaemon != 0 && ld->fStdio != 0)
   {  fprintf(stderr, "Can't daemonize in console mode\n");
      fDaemon = 0;
   }

// Load gates table (check file integrity)
   rc = reslinks_load(LOCK_SH);
   if (rc != SUCCESS)
   {  syslog(LOG_ERR, "Can't load gates file");
      exit (-1);
   }

   if (fDumpGates)
   {
      printf("SET client_encoding = 'KOI8';\n\n");
      printf("TRUNCATE beegates;\n");
      printf("COPY beegates (res_id, rid, acc_id, ident) FROM stdin;\n");
      for (i=0; i < linktabsz; i++)
      {
         printf ("%d\t%d\t%d\t%s\n", 
                   linktab[i].res_id,
                   linktab[i].user_id,
                   linktab[i].accno,
                   linktab[i].username);
      }
      printf("\\.\n");
      
      rc = acc_baseopen(&Accbase, conf_accfile);
      if (rc < 0)
      {  syslog(LOG_ERR, "Can't open account database");
         exit (-1);
      }

      printf("TRUNCATE beeaccs;\n");
      printf("COPY beeaccs (id, tag_deleted, tag_broken, tag_frozen, tag_off, tag_unlim, tag_payman, tag_log, "
             "balance, tariff_id, inetin_summ, inetout_summ, money_summ, tcredit "
             ") FROM stdin;\n");

      c = acc_reccount(&Accbase);

      for (i=0; i < c; i++)
      {
         rc = acc_get(&Accbase, i, &new_acc);  
         if (rc < 0 && rc > ACC_BROKEN) continue;

         printf ("%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%g\t%d\t%lld\t%lld\t%g\t%g\n",
                   i, 
                   new_acc.tag & ATAG_DELETED ? "t":"f",
                   new_acc.tag & ATAG_BROKEN  ? "t":"f",
                   new_acc.tag & ATAG_FROZEN  ? "t":"f",
                   new_acc.tag & ATAG_OFF     ? "t":"f",
                   new_acc.tag & ATAG_UNLIMIT ? "t":"f",
                   new_acc.tag & ATAG_PAYMAN  ? "t":"f",
                   new_acc.tag & ATAG_LOG     ? "t":"f",
                   new_acc.balance,
                   new_acc.tariff,
                   new_acc.inet_summ_in,
                   new_acc.inet_summ_out,
                   new_acc.money_summ,
                   new_acc.tcredit);
      }
      printf("\\.\n");
       
      exit(0);
   }

// Open database (to check & update if needed)
   rc = acc_baseopen(&Accbase, conf_accfile);
   if (rc < 0)
   {  syslog(LOG_ERR, "Can't open account database");
      exit (-1);
   }   
   rc = log_baseopen(&Logbase, conf_logfile);
   if (rc < 0)
   {  syslog(LOG_ERR, "Can't open log database");
      exit (-1);
   }   
   rc = tariffs_load(conf_tariffile);
   if (rc < 0)
   {  syslog(LOG_ERR, "Can't open tariff database");
      exit (-1);
   }   

   if (fSQL)
   {
      fprintf(stderr, "SQL init ... ");

   // Initialize connection
      DBdata = db2_init(config.DBtype, config.DBhost, config.DBname);
      if (DBdata == NULL)
      {  fprintf(stderr, "FAULT. db_init() fails.\n");
         exit (-1);
      }
   // Open database
      rc = db2_open(DBdata, config.DBlogin, config.DBpass);
      if (rc < 0)
      {  fprintf(stderr, "FAULT. db_open() fails.\n");
         exit (-1);
      }

      fprintf(stderr, "OK.\n");

// Perform immediate query from file
      if (QueryFilename != NULL)
      {  fd = open(QueryFilename, O_RDONLY, 0);
         if (fd >= 0)
         {  c = 0;
            rc = ioctl(fd, FIONREAD, &c);
            if (rc >= 0 && c > 1)
            {  QueryBuffer = (char*)calloc(1, c + 1);
               if (QueryBuffer != NULL)
               {  rc = read(fd, QueryBuffer, c);
                  if (rc == c)
                  {  if (DumpQuery) printf("%s", QueryBuffer);
                     rc = db2_execute(DBdata, "%s", QueryBuffer);
                     if (rc < 0) fprintf(stderr, "QUERY: SQL error\n");
                     else fprintf(stderr, "QUERY: done.\n"); 
                  }
                  else fprintf(stderr, "QUERY: File read error\n");
                  free(QueryBuffer);
                  QueryBuffer = NULL;
               }
               else fprintf(stderr, "QUERY: Memory allocation error\n");
            }
            else fprintf(stderr, "QUERY: File size error\n");
            close(fd);
         }
         else fprintf(stderr, "QUERY: File access error - %s\n", strerror(errno));
      }

// Perform test query
      row = db2_search(DBdata, 0, "SELECT count(*) FROM accs");
      if (row == NULL)
      {  fprintf(stderr, "Test fails.\n");
         exit (-1);
      }
      else fprintf(stderr, "Test OK. (%s accounts).\n", *row);
  
   } // if fSQL

// Update resources permissions
   if (fUpdate != 0) access_update(); 

// Close database
   acc_baseclose(&Accbase);
   log_baseclose(&Logbase);
   tariffs_free();

// Close database
   if (fSQL)
   {
      rc = db2_close(DBdata);
      if (rc < 0)
      {  fprintf(stderr, "FATAL: db2_close() fails.\n\n");
         exit (-1);
      }
   } // if fSQL

// Start server
   if (fRun)
   {  if (fDaemon)
      {  rc = daemon(0, 0);
         if (rc != SUCCESS)
         {  syslog(LOG_ERR, "Can't daemonize, closing");
            exit(-1);
         }
         if (proc_title == NULL) setproctitle("(daemon)");
         else setproctitle(proc_title);
      }
      if (setenv("HOME", "/root", 0) == (-1)) syslog(LOG_ERR, "setenv(): %m");
      rc = link_wait(ld, ForceService < 0 ? conf_coreport : ForceService);
      if (rc != -1)
      {  
         
         if (ld->fStdio == 0) setproctitle("(child)");
         else 
         {  if (proc_title == NULL) setproctitle("(console)");
            else setproctitle(proc_title);
         }

         cmd_out(RET_COMMENT, "Billing ver %d.%d.%d.%d", VER_VER, VER_SUBVER, VER_REV, VER_SUBREV);
         cmd_out(RET_COMMENT, "loading configuration ...");
         conf_free();
         rc = conf_load(Config_path);
         if (rc < 0)
         {  cmd_out(ERR_IOERROR, "failure");
            exit(-1);
         }
         cmd_out(RET_COMMENT, "loading gates ...");
// ReLoad linktable
         rc = reslinks_load(LOCK_SH);
         if (rc != SUCCESS)
         {  cmd_out(ERR_IOERROR, "failure: %s", strerror(errno));
            exit(-1);
         }
// Open database
         cmd_out(RET_COMMENT, "opening accounts ...");
         rc = acc_baseopen(&Accbase, conf_accfile);
         if (rc < 0)
         {  syslog(LOG_ERR, "Can't reopen account database");
            cmd_out(ERR_IOERROR, "failure: %s", strerror(errno));
            exit(-1);
         }
         cmd_out(RET_COMMENT, "opening log ...");
         rc = log_baseopen(&Logbase, conf_logfile);
         if (rc < 0)
         {  syslog(LOG_ERR, "Can't reopen log database");
            cmd_out(ERR_IOERROR, "failure: %s", strerror(errno));
            exit(-1);
         }
         cmd_out(RET_COMMENT, "loading tariffs ...");
         rc = tariffs_load(conf_tariffile);
         if (rc < 0)
         {  syslog(LOG_ERR, "Can't reopen tariff database");
            cmd_out(ERR_IOERROR, "failure");
            exit(-1);
         }

         if (fSQL)
         {
            cmd_out(RET_COMMENT, "connecting SQL ...");
            rc = db2_open(DBdata, config.DBlogin, config.DBpass);
            if (rc < 0)
            {  syslog(LOG_ERR, "Can't reopen SQL connection");
               cmd_out(ERR_IOERROR, "FAILURE");
               exit(-1);
            }

/*
            if (ld->fStdio)
            {  row = dbex_search(DBdata, 0, "log_session");
               if (row == NULL || row[0] == NULL)
               {  cmd_out(ERR_SYSTEM, "Session open failure");
                exit(-1);
               }
               SessionId = strtoll(row[0], NULL, 10);
               if (SessionId < 1)
               {  cmd_out(ERR_SYSTEM, "Session open failure (invalid id returned)");
                  exit(-1);
               }
               cmd_out(RET_COMMENT, "Log session (id: %lld)", SessionId);

               SessionPerm  = PERM_SUPERUSER;
               strlcpy(SessionLogin, "admin", sizeof(SessionLogin));
               UserId = 1;
               cmd_out(RET_COMMENT, "Root autologin ...");
            }
*/
         } // if fSQL
         cmd_out(RET_SUCCESS, "Ready");
         while(1)
         {  rc = link_gets(ld, sbuf, sizeof(sbuf));
            if (rc == LINK_DOWN) break;
            if (rc == SUCCESS) 
            {  rc = cmd_exec(sbuf, NULL);
               if (rc == LINK_DOWN || rc == CMD_EXIT) break;
            }
         }
      }
   }

// Close SQL connection
   if (fSQL) db2_close(DBdata);

   acc_baseclose(&Accbase);
   log_baseclose(&Logbase);

   return 0;
}

void usage(int code)
{
   syslog(LOG_ERR, "usage: %s %s", __progname, OPTS);
   closelog();

   exit(code);
}

int access_update()
{  FILE *     f[resourcecnt];
   FILE *    f2[resourcecnt];
   FILE *    fil;
   char      filename[64];
   int       accs;
   int       i;
   int       ind;
   int       lockfd;
   acc_t     acc;
   int       rc;
   timeval_t locktimer;
   
// open lockfile
   lockfd = open(conf_updlock, O_CREAT);
   if (lockfd == NULL)
   {  syslog(LOG_ERR, "access_update(open(lockfile)): %m");
      return (-1);
   }

// try to lock lockfile (w/timeout)
   tm_sets(&locktimer, 3);          // 3 seconds timeout

   rc = (-1);                       // for safety 
   while(tm_state(&locktimer))
   {  rc = flock(lockfd, LOCK_EX | LOCK_NB);
      if (rc >= 0) break;
      if (errno != EWOULDBLOCK) break;
      usleep(10000);
   } 
   
// fail on error
   if (rc < 0)
   {  syslog(LOG_ERR, "access_update(flock): %m");
      close(lockfd);
      return (-1);
   }

   for (i=0; i < resourcecnt; i++)
   {  snprintf(filename, sizeof(filename), 
               conf_grantmask, resource[i].name);
      if (resource[i].count != NULL)
      {  f[i] = fopen(filename, "w");
         if (f[i] == NULL) syslog(LOG_ERR, "fopen(%s): %m", filename);
      }
      else f[i] = NULL;

      snprintf(filename, sizeof(filename), 
               conf_denymask, resource[i].name);
      if (resource[i].count != NULL)
      {  f2[i] = fopen(filename, "w");
         if (f2[i] == NULL) syslog(LOG_ERR, "fopen(%s): %m", filename);
      }
      else f2[i] = NULL;
   }
   accs = acc_reccount(&Accbase);

   for (i=0; i < accs; i++)
   {  rc = acc_get(&Accbase, i, &acc);
      if (rc == ACC_UNLIMIT) rc = SUCCESS;
      else
      {  if (rc == SUCCESS || rc == NEGATIVE) rc = accs_state(&acc);
      }
      ind = -1;
      while (lookup_accno(i, &ind) >= 0)
      {  if (f[linktab[ind].res_id] != NULL)
         {  if (rc == SUCCESS && linktab[ind].allow) 
                 fil =  f[linktab[ind].res_id];
            else fil = f2[linktab[ind].res_id];
            if (fil != NULL) fprintf(fil, "%s\n", linktab[ind].username);
         }
      }
   }   

   for (i=0; i < resourcecnt; i++) 
   {  if (f[i]  != NULL) fclose( f[i]);
      if (f2[i] != NULL) fclose(f2[i]);
   } 

   for (i=0; i < resourcecnt; i++)
   {  if (resource[i].ruler_cmd != NULL)
      {  rc = system(resource[i].ruler_cmd);
         switch (rc)
         {  case (-1):
              syslog(LOG_ERR, "system(%s): %m", resource[i].ruler_cmd);
              break;

            case 127:
              syslog(LOG_ERR, "system(%s): sh(1) fails", resource[i].ruler_cmd);
              break;

            default:
              if (rc != NULL) 
                 syslog(LOG_ERR, "%s ret=%d", resource[i].ruler_cmd, rc);
         }
      } 
   }

// Prior lock release
   flock(lockfd, LOCK_UN);
   close(lockfd);

   rc = system(conf_applyscript);

   switch (rc)
   {  case (-1):
        syslog(LOG_ERR, "system(%s): %m", conf_applyscript);
        break;

      case 127:
        syslog(LOG_ERR, "system(%s): sh(1) fails", conf_applyscript);
        break;

      default:
        if (rc != NULL) 
          syslog(LOG_ERR, "%s ret = %d", conf_applyscript, rc);
   }

//   flock(lockfd, LOCK_UN);
//   close(lockfd);


   return 0;
}

#define LogStep 1

int acc_transaction (accbase_t * base, logbase_t * logbase, int accno, is_data_t * isdata, double realarg, double limit)
{  long long rc;
   acc_t     acc;
   logrec_t  logrec;
   logrec_t  oldrec;
   long long i;
   long long recs;
   double    sum = 0;

   memset(&logrec, 0, sizeof(logrec));

   if (isdata == NULL) return IO_ERROR; // todo: system error

   rc = acc_baselock(base);
   if (rc >= 0)
   {
// log fixed fields
      logrec.time  = time(NULL);   // stub

      logrec.accno = accno;
// get account
      for (i=0; i < 3; i++)
         if ((rc = acci_get(base, accno, &acc)) != IO_ERROR) break;
      logrec.serrno = rc;
// if account is not broken, store balance
      if (rc >= 0 || rc <= ACC_FROZEN) logrec.balance = acc.balance;

// Count transaction sum
      if (isdata->res_id == RES_ADDER)
      {  sum = realarg - (limit && acc.balance + realarg > limit ? (acc.balance + realarg - limit) : 0);
      }   
      else
      {  if (realarg == (-1)) 
            sum = resource[isdata->res_id].count(isdata, &acc); 
         else
            sum = - ((double)isdata->value * realarg / 1048576);
      }

      logrec.sum    = sum;
      logrec.isdata = *isdata; // store is_data w/changes from count proc

// if account in valid (not frozen) count new balance
      if (rc == SUCCESS || rc == NEGATIVE || rc == ACC_OFF ||
           (rc == ACC_UNLIMIT && isdata->res_id == RES_ADDER)) 
      {
         acc.balance += sum;
// reset temp credit
         if (isdata->res_id == RES_ADDER) 
         {  acc.tcredit     = 0;
            acc.res_tcredit = 0;
         }
      }

      if ((acc.tag & (ATAG_UNLIMIT | ATAG_LOG)) == ATAG_UNLIMIT && isdata->res_id != RES_ADDER)
         logrec.sum = 0;

// if account is valid - write account back
      if (rc >= 0 || rc == ACC_OFF)
      {  for (i=0; i<3; i++)
            if ((rc = acci_put(base, accno, &acc)) != IO_ERROR) break;
         // if write insuccess - log error
         if (rc < 0) logrec.serrno = rc;
      }

/*
      if ((rc = log_baselock(logbase)) == SUCCESS)
      {  recs = log_reccount(logbase);
         rc   = (-1);
         for (i = recs - 1; i >= 0; i--)
         {  if ((rc = logi_get(logbase, i, &oldrec)) == SUCCESS)
            {  if (logrec.time/LogStep == oldrec.time/LogStep)
               {  if (logrec.accno == oldrec.accno                 &&
                      logrec.serrno == oldrec.serrno                 &&
                  logrec.isdata.res_id == oldrec.isdata.res_id     &&
                  logrec.isdata.user_id == oldrec.isdata.user_id   &&
                  logrec.isdata.proto_id == oldrec.isdata.proto_id &&
                  logrec.isdata.proto2 == oldrec.isdata.proto2     &&
                  logrec.isdata.host.s_addr == oldrec.isdata.host.s_addr &&
                  ((long long)logrec.isdata.value) + ((long long)oldrec.isdata.value) < UINT_MAX)
                  {  oldrec.isdata.value += logrec.isdata.value;
                     oldrec.sum += logrec.sum;
                     oldrec.balance = BALANCE_NA;
                     rc = logi_put(logbase, i, &oldrec);
                     break;
                  }
                  rc = (-1);
               }
               else
               {  rc = (-1);
                  break;
               }
            }
         }
         log_baseunlock(logbase);
      }
      if (rc < 0) 
*/
      rc = log_add(logbase, &logrec);
      acc_baseunlock(base);
   } else return IO_ERROR;
// return error or account state

   if (rc < 0) return rc;

// TODO: fix invalid return code

   return accs_state(&acc);
}

double acc_limit(acc_t * acc)
{  int limit = 0; // default limit
   int i = 0;

   if (acc->tcredit != 0) return acc->tcredit;

   for (i = 0; tariffs_limit[i].tariff >= 0; i++)
   {  if (acc->tariff == tariffs_limit[i].tariff)
      {  limit = i;
         break;
      } 
   }

   return tariffs_limit[limit].min;
}

int accs_state(acc_t * acc)
{   double limit;

   limit = acc_limit(acc);

   return (acc->balance < (limit + 0.01));
}

int acc_charge_trans (accbase_t * base, logbase_t * logbase, int accno, is_data_t * isdata)
{  long long rc;
   acc_t     acc;
   logrec_t  logrec;
   logrec_t  oldrec;
   long long i;
   long long recs;
   double    sum = 0;

   memset(&logrec, 0, sizeof(logrec));

   rc = acc_baselock(base);
   if (rc >= 0)
   {
// log fixed fields
      logrec.time  = time(NULL);   // stub
      logrec.accno = accno;
      if (isdata != NULL) logrec.isdata = *isdata;
      else  logrec.isdata.res_id = (-1);
// get account
      for (i=0; i < 3; i++)
         if ((rc = acci_get(base, accno, &acc)) != IO_ERROR) break;
      logrec.serrno = rc;
// if account is not broken, store balance
      if (rc >= 0 || rc <= ACC_FROZEN) logrec.balance = acc.balance;

// do not charge daily fee if account is not valid or frozen
      if ((acc.tag & (ATAG_DELETED | ATAG_BROKEN | ATAG_FROZEN)) != 0) return ACC_NOCHARGE;

// do not charge daily fee if account is negative
      if ( acc.balance < (acc_limit(&acc) + 0.01) ) return ACC_NOCHARGE;

// Count charge transaction sum
      sum = resource[isdata->res_id].charge(&acc);

      logrec.sum   = sum;

// if account in valid (not frozen) count new balance
      if (logrec.sum != 0)
      {  if (rc == SUCCESS || rc == NEGATIVE || rc == ACC_OFF) acc.balance += sum;
// if account is valid - write account back
         if (rc >= 0 || rc == ACC_OFF)
         {  for (i=0; i < 3; i++)
               if ((rc = acci_put(base, accno, &acc)) != IO_ERROR) break;
            // if write insuccess - log error
            if (rc < 0) logrec.serrno = rc;
         }
/*
         if ((rc = log_baselock(logbase)) == SUCCESS)
         {  recs = log_reccount(logbase);
            rc   = (-1);
            for (i = recs-1; i >= 0; i--)
            {  if ((rc = logi_get(logbase, i, &oldrec)) == SUCCESS)
               {  if (logrec.time/LogStep == oldrec.time/LogStep)
                  {  if (logrec.accno == oldrec.accno                 &&
                         logrec.serrno == oldrec.serrno                 &&
                     logrec.isdata.res_id == oldrec.isdata.res_id     &&
                     logrec.isdata.user_id == oldrec.isdata.user_id   &&
                     logrec.isdata.proto_id == oldrec.isdata.proto_id &&
                     logrec.isdata.proto2 == oldrec.isdata.proto2     &&
                     logrec.isdata.host.s_addr == oldrec.isdata.host.s_addr)
                     {  oldrec.isdata.value += logrec.isdata.value;
                        oldrec.sum += logrec.sum;
                        oldrec.balance = BALANCE_NA;
                        rc = logi_put(logbase, i, &oldrec);
                        break;
                     }
                     rc = (-1);
                  }
                  else
                  {  rc = (-1);
                     break;
                  }
               }
            }
            log_baseunlock(logbase);
         }
         if (rc < 0)
*/
            rc = log_add(logbase, &logrec);
      } 
      else rc = ACC_NOCHARGE;

      acc_baseunlock(base);
   } else return IO_ERROR;
// return error or account state

   if (rc < 0) return rc;

   return accs_state(&acc);
}


int acc_bitrans      (accbase_t * base, logbase_t * logbase, int accno, is_data_t * isdata, u_int val_in, u_int val_out, double realarg, double limit)
{  long long rc;
   acc_t     acc;
   logrec_t  logrec_in;
   logrec_t  logrec_out;
   long long i;
   long long recs;
   double    sum  = 0;

   memset(&logrec_in,  0, sizeof(logrec_in));
   memset(&logrec_out, 0, sizeof(logrec_out));

   if (isdata == NULL) return IO_ERROR; // todo: system error

   rc = acc_baselock(base);
   if (rc >= 0)
   {
// log fixed fields
      logrec_in.time  = time(NULL);   // stub

      logrec_in.accno = accno;
// get account
      for (i=0; i < 3; i++)
         if ((rc = acci_get(base, accno, &acc)) != IO_ERROR) break;
      logrec_in.serrno = rc;

// if account is not broken, store balance
      if (rc >= 0 || rc <= ACC_FROZEN) logrec_in.balance = acc.balance;

// Copy to second log record
      logrec_out = logrec_in;
      if (val_in != 0) logrec_out.balance = BALANCE_NA;

// Count transaction sum IN
      if (val_in != 0)
      {  isdata->value    = val_in;
         isdata->proto_id = 0;
         sum = resource[isdata->res_id].count(isdata, &acc); 

         logrec_in.sum    = sum;
         logrec_in.isdata = *isdata;   // store is_data w/changes from count proc

   // if account in valid (not frozen) count new balance
         if (rc == SUCCESS || rc == NEGATIVE || rc == ACC_OFF ||
              (rc == ACC_UNLIMIT && isdata->res_id == RES_ADDER)) 
         {
            acc.balance += sum;
         }
      }

// Count transaction sum OUT
      if (val_out != 0)
      {  isdata->value    = val_out;
         isdata->proto_id = 2147483648u;  // outbound
         sum = resource[isdata->res_id].count(isdata, &acc); 

         logrec_out.sum    = sum;
         logrec_out.isdata = *isdata;   // store is_data w/changes from count proc

  // if account in valid (not frozen) count new balance
         if (rc == SUCCESS || rc == NEGATIVE || rc == ACC_OFF ||
              (rc == ACC_UNLIMIT && isdata->res_id == RES_ADDER)) 
         {
            acc.balance += sum;
         }
      }

      if ((acc.tag & (ATAG_UNLIMIT | ATAG_LOG)) == ATAG_UNLIMIT && isdata->res_id != RES_ADDER)
         logrec_in.sum = logrec_out.sum = 0;

// if account is valid - write account back
      if (rc >= 0 || rc == ACC_OFF)
      {  for (i=0; i<3; i++)
            if ((rc = acci_put(base, accno, &acc)) != IO_ERROR) break;
         // if write insuccess - log error
         if (rc < 0) logrec_in.serrno = logrec_out.serrno = rc;
      }

      acc_baseunlock(base);

      if ((rc = log_baselock(logbase)) == SUCCESS)
      {
          if (val_in  != 0) rc = log_add(logbase, &logrec_in);
          if (val_out != 0) rc = log_add(logbase, &logrec_out);
          log_baseunlock(logbase);
      }

   } else return IO_ERROR;
// return error or account state

   if (rc < 0) return rc;

// TODO: fix invalid return code

   return accs_state(&acc);
}


