#include <sys/types.h>
#include <time.h>

#include "global.h"


char * TitleText = "���e���� ����� (���. 0.0)";

char * months_cased[]=
{  "������",
   "�������", 
   "�����", 
   "������", 
   "���", 
   "����", 
   "����", 
   "�������", 
   "��������", 
   "�������", 
   "������", 
   "�������"
};

char * months[]=
{  "������",
   "�������", 
   "����", 
   "������", 
   "���", 
   "����", 
   "����", 
   "������", 
   "��������", 
   "�������", 
   "������", 
   "�������"
};

char date_buf[64];

int RefreshTitle()
{  int i;
   int n;

   time_t     tim;
   struct tm  stm;

   tim = time(NULL);
   if (localtime_r(&tim, &stm) != NULL)
   {  snprintf(date_buf, sizeof(date_buf), "%d %s %d ���� ",
      stm.tm_mday, months_cased[stm.tm_mon], stm.tm_year + 1900);
   }
   else date_buf[0]='\0';

   Attr(8, 7);
   Gotoxy(0,0);

// Output centered text
   n = (ForceCols - strlen(TitleText)) / 2;
   for(i = 0; i < n; i++) Putch(' ');

   Puts(TitleText);   

   n = (ForceCols - strlen(TitleText)) / 2 + 
       (ForceCols - strlen(TitleText)) % 2 -
       strlen(date_buf);
   for(i = 0; i < n; i++) Putch(' ');

   Puts(date_buf);
   
   return 0;
}
