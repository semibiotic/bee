/* $Bee$ */
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <syslog.h>

#include <bee.h>
#include <ipc.h>
#include <db.h>

link_t	     lnk;
char       * host = BEE_ADDR; 
int          port = BEE_SERVICE;

int
main(argc, argv)
   int  argc;
   char **argv;
{
   int      tag;
   int      balance;
   int      rc;
   char   * msg;
   char   * ptr;
   char   * str;
   int      colon;
   char     linbuf[128];

   openlog("getacc", LOG_PID | LOG_NDELAY, LOG_DAEMON);

   if (argc < 2)
   {  printf("C������� � ����� ����������: ������ �������\n"); 
      return (-1);
   }

   rc = link_request(&lnk, host, port); 
   if (rc == -1)
   {  syslog(LOG_ERR, "Can't connect to billing service: %m"); 
      printf("C������� � ����� ����������: ������ �� �������\n"); 
      exit(-1); 
   }

   rc = answait(&lnk, RET_SUCCESS, linbuf, sizeof(linbuf), &msg);
   if (rc != RET_SUCCESS)
   {  if (rc == LINK_DOWN) syslog(LOG_ERR,"Unexpected link down");
      if (rc == LINK_ERROR) syslog(LOG_ERR, "Link error: %m");
      if (rc >= 400) syslog(LOG_ERR, "Billing error: %s", msg);
      printf("C������� � ����� ����������: ������ �������\n"); 
      exit(-1);
   }

   link_puts(&lnk, "machine");
   rc = answait(&lnk, RET_SUCCESS, linbuf, sizeof(linbuf), &msg);
   if (rc != RET_SUCCESS)
   {  if (rc == LINK_DOWN) syslog(LOG_ERR,"Unexpected link down");
      if (rc == LINK_ERROR) syslog(LOG_ERR, "Link error: %m");
      if (rc >= 400) syslog(LOG_ERR, "Billing error: %s", msg);
      printf("C������� � ����� ����������: ������ �������\n"); 
      exit(-1);
   }

   link_puts(&lnk, "acc inet name %s", argv[1]);
   rc = answait(&lnk, RET_STR, linbuf, sizeof(linbuf), &msg);
   if (rc != RET_STR && rc < 400)
   {  if (rc == RET_SUCCESS) syslog(LOG_ERR, "Unexpected SUCCESS");
      if (rc == LINK_DOWN) syslog(LOG_ERR,"Unexpected link down");
      if (rc == LINK_ERROR) syslog(LOG_ERR, "Link error: %m");
      printf("C������� � ����� ����������: ������ �������\n"); 
      exit(-1);
   }
   switch (rc)
   {  case RET_STR:
         ptr=msg;
         str=next_token(&ptr, " \t");
         if (str == NULL)
         {  printf("C������� � ����� ����������: ������ �������\n");
            break;
         }
         printf("C��� #%04ld ", strtol(str, NULL, 0));
         str=next_token(&ptr, " \t");
         if (str == NULL)
         {  printf("������ �������\n");
            break;
         }
         tag=strtol(str, NULL, 0);
         colon=0;
         if (tag != 0) printf("(");
         if (tag & ATAG_DELETED) 
         {  printf("c��� ������)\n");
            break;
         }
         if (tag & ATAG_BROKEN)
         {  printf("c��� ���������)\n");
            break;
         }
         if (tag & ATAG_UNLIMIT)
         {  if (colon) printf(", ");
            printf("�������������� ������");
            colon=1;
         }
         if (tag & ATAG_FROZEN)
         {  if (colon) printf(", ");
            printf("���������");
            colon=1;
         }
         if (tag & ATAG_OFF)
         {  if (colon) printf(", ");
            printf("�������������");
            colon=1;
         }
         if (tag != 0) printf(") ");
         str=next_token(&ptr, " \t");
         if (str == NULL)
         {  printf("������ �������\n");
            break;
         }
         balance=strtod(str, NULL) * 100;
         if (balance < 0) printf("����: ");
         else printf("�������: ");
         balance=abs(balance);
         printf("%d ���. %d ���.\n", balance/100, balance%100);
         break; 
      case ERR_NOACC:
         printf("������ ����� �� ����������\n");
         exit(-1); 
      default:
         printf("C������� � ����� ����������: ������ �������\n");
         exit(-1); 
   }

   link_puts(&lnk, "exit");
   link_close(&lnk);
   return 0;
}
