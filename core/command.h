/* $RuOBSD: command.h,v 1.1 2007/09/23 19:49:12 shadow Exp $ */

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdarg.h>

#define CMD_EXIT (-101)

#define CMD_DELIM " \t\n\r"

#define LF_NONE       0
#define LF_NAME       1
#define LF_ADDR       2
#define LF_ALL        3
#define LF_PNAME      4

typedef int (*cmd_proc_t)(char * cmd, char * args);

typedef struct _command_t command_t;
struct _command_t
{  char       * ident;
   cmd_proc_t   proc;
   u_int        pl;
   command_t  * subtab;
};

typedef struct
{  int    what;
   char * str;
   int    ind;
   int    no;
} lookup_t;

// execute (sub-)command (find & execute handler)
int cmd_exec  (char * str, command_t * table);

int cmd_out   (int err, char * format, ...);
int cmd_getaccno (char ** args, lookup_t * prev);
int cmd_ptime (time_t utc, char * buf);
int cmd_add   (int accno, money_t sum);
int cmd_accerr(int rc);
time_t cmd_gettime(char ** arg, time_t tim);
int cmd_plogrec(logrec_t * logreg);
int cmd_pdate(time_t tim, char * buf);
int cmd_getdate(char ** pptr);

int cmdh_exit   (char * cmd, char * args);
int cmdh_notimpl(char * cmd, char * args);
int cmdh_ver    (char * cmd, char * args);
int cmdh_new    (char * cmd, char * args);
int cmdh_acc    (char * cmd, char * args);
int cmdh_freeze (char * cmd, char * args);
int cmdh_fix    (char * cmd, char * args);
int cmdh_acc    (char * cmd, char * args);
int cmdh_log    (char * cmd, char * args);
int cmdh_res    (char * cmd, char * args);
int cmdh_hres    (char * cmd, char * args);
int cmdh_add	(char * cmd, char * args);
int cmdh_update	(char * cmd, char * args);
int cmdh_human	(char * cmd, char * args);
int cmdh_lookup (char * cmd, char * args);
int cmdh_date   (char * cmd, char * args);
int cmdh_report (char * cmd, char * args);
int cmdh_delete (char * cmd, char * args);
int cmdh_new_contract (char * cmd, char * args);
int cmdh_new_name (char * cmd, char * args);
int cmdh_new_vpn  (char * cmd, char * args);
int cmdh_gate (char * cmd, char * args);
int cmdh_delgate (char * cmd, char * args);
int cmdh_intraupdate (char * cmd, char * args);
int cmdh_setstart (char * cmd, char * args);
int cmdh_lock   (char * cmd, char * args);
int cmdh_accres (char * cmd, char * args);
int cmdh_docharge(char * cmd, char * args);
int cmdh_tdump(char * cmd, char * args);


#endif /* __COMMAND_H__ */