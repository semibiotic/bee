/* $RuOBSD: links.c,v 1.18 2009/11/28 10:14:46 shadow Exp $ */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <bee.h>
#include <db.h>
#include <links.h>
#include <res.h>

reslink_t * linktab   = NULL;
int         linktabsz = 0;

static char addrbuf1[32];

#define DELIM  " ,\t\n\r"

int reslinks_lock(int locktag)
{  int lockfd;

   lockfd = open(conf_gatelock, O_CREAT);
   if (lockfd == -1)
   {  syslog(LOG_ERR, "reslinks_lock(open(%s)): %m", conf_gatelock);
      return (-1);
   }

   if (flock(lockfd, locktag) == -1)  
   {  syslog(LOG_ERR, "reslinks_lock(flock): %m");
      return (-1);
   }
   return lockfd;
}

int reslinks_unlock(int lockfd)
{
   if (flock(lockfd, LOCK_UN) == -1)
   {  syslog(LOG_ERR, "reslinks_unlock(flock): %m");
      return (-1);
   }

   close(lockfd);

   return 0;
}

int reslinks_load(int locktag)
{  FILE      * fd;
   char        buf[128];
   char      * ptr;
   char      * str;
   reslink_t   worksp;
   void      * tmp;
   int         lin    = 0;
   int         fdlock = (-1);
   int         i, rc;

   if (locktag != LOCK_UN) 
   {  fdlock = reslinks_lock(locktag);
      if (fdlock == -1) return (-1);
   }  

   if (linktab != NULL) 
   {  free(linktab);
      linktab = NULL;
   }
   linktabsz = 0;

   fd = fopen(conf_gatefile, "r");
   if (fd == NULL)
   {  syslog(LOG_ERR, "reslinks_load(fopen(%s)): %m", conf_gatefile);
      if (fdlock != -1) reslinks_unlock(fdlock);
      return (-1);
   }

   while(fgets(buf, sizeof(buf), fd) != NULL)
   {  lin++;
// comment check
      if (*buf == '#') continue;
      ptr = buf;
// zero fill srtucture 
      bzero(&worksp, sizeof(worksp));
// stub for gate allowance
      worksp.allow = 1;
// get resource name
      str = next_token(&ptr, DELIM);
      if (str == NULL) continue;  // skip empty line
// tranlate res.name -> res.id
      for (i=0; i<resourcecnt; i++)
        if (strcmp(resource[i].name, str) == 0) break;
      if (i == resourcecnt) 
      {  syslog(LOG_ERR, "%s: %d: Unknown resource name \"%s\"", 
                 conf_gatefile, lin, str);  
         continue;
      }
      else worksp.res_id = i; 
// Parse 2 int values (user_id, accno)
      for (i=1; i<3; i++)
      {  str = next_token(&ptr, DELIM);
         if (str == NULL) break;
         ((int *)&worksp)[i] = strtol(str, NULL, 10);
      }
      if (i < 3)
      {  syslog(LOG_ERR, "%s: %d: Unexpected line end", conf_gatefile, lin);
         continue;
      }  
// Get username/address 
      str = strtrim(ptr, DELIM);
      if (str == NULL || *str == '\0')
      {  syslog(LOG_ERR, "%s: %d: Unexpected line end", conf_gatefile, lin);
         continue;
      }
      tmp = calloc(1, strlen(str)+1);
      if (tmp == NULL)
      {  syslog(LOG_ERR, "reslinks_load(calloc): %m");
         fclose(fd);
         if (fdlock != -1) reslinks_unlock(fdlock);
         return (-1);
      }
      worksp.username = (char*)tmp;
      strcpy(worksp.username, str);
// Make address/mask binary values
      if (worksp.res_id == RES_INET)
      {  rc = make_addrandmask(worksp.username, &(worksp.addr), 
               &(worksp.mask));
      // Skip invalid items
         if (rc < 0)
         {  syslog(LOG_ERR, "%s: %d: Invalid inet ident '%s'", conf_gatefile, lin, worksp.username);
            worksp.username = NULL;
            continue;
         }

      // Recreate inet ident string
         free(worksp.username);
         worksp.username = NULL;
         asprintf(((char**)&tmp), "%s/%d", inet_ntop(AF_INET, &(worksp.addr), addrbuf1, sizeof(addrbuf1)), mask2bits(worksp.mask));
         if (tmp == NULL)
         {  syslog(LOG_ERR, "reslinks_load(calloc): %m");
            fclose(fd);
            if (fdlock != -1) reslinks_unlock(fdlock);
            return (-1);
         }
         worksp.username = (char*)tmp;

      // Check new item for intersection (disable intersecting gates)
         rc = lookup_intersect(worksp.addr, worksp.mask, NULL);
         if (rc >= 0)
         {  syslog(LOG_ERR, "INTERSECTION - new gate (%s for #%d) intersects old one (%s for #%d), disabling gate",
                   worksp.username, worksp.accno, linktab[rc].username, linktab[rc].accno);
            worksp.allow = 0;
         }        
      }

// Add item
      tmp = realloc(linktab, (linktabsz+1)*sizeof(reslink_t));
      if (tmp == NULL)
      {  syslog(LOG_ERR, "reslinks_load(realloc): %m");
         fclose(fd);
         if (fdlock != -1) reslinks_unlock(fdlock);
         return (-1);
      }
      linktab = (reslink_t*)tmp;
      linktab[linktabsz++] = worksp;
   } // while()
   fclose(fd);
   if (fdlock != -1) reslinks_unlock(fdlock);
   return 0;
}

int reslinks_save(int locktag)
{  FILE * fd;
   int    lockfd = -1;
   int    i;
   int    rc;

   if (locktag != LOCK_UN) 
   {  lockfd = reslinks_lock(locktag);
      if (lockfd == -1) return (-1);
   }  

   fd = fopen(conf_gatetemp, "w"); 
   if (fd == NULL)
   {  syslog(LOG_ERR, "reslinks_save(fopen): %m");
      if (lockfd != -1) reslinks_unlock(lockfd);
      return -1;
   }

   rc = fprintf(fd, "# Created by bee\n"
           "#\tres\tuid\tacc\tname\n");
   if (rc >= 0)
   {  for (i=0; i<linktabsz; i++)
      {  rc = fprintf(fd, "\t%s\t%d\t%d\t%s\n",
              resource[linktab[i].res_id].name,
              linktab[i].user_id,
              linktab[i].accno,
              linktab[i].username);
         if (rc < 0) break;
      }
   }
   fclose(fd);
   if (rc < 0) 
   {  syslog(LOG_ERR, "reslinks_save(fprintf): %m");
      if (lockfd != -1) reslinks_unlock(lockfd);
      return -1;
   }
   rename(conf_gatetemp, conf_gatefile);

   if (lockfd != -1) reslinks_unlock(lockfd);
   return 0;
}

int reslink_new(int rid, int accno, char * name)
{  int        i;
   int        uid      = (-1);
   reslink_t  newlink;
   void     * tmp;
   int        rc;

// initialize new gate     
   bzero(&newlink, sizeof(newlink));
   newlink.res_id = rid;
   newlink.accno  = accno;
   newlink.allow  = 1;
   tmp = calloc(1, strlen(name) + 1);
   if (tmp == NULL)
   {  syslog(LOG_ERR, "reslink_new(calloc): %m");
      return (-1);
   }
   strcpy((char *)tmp, name);
   newlink.username = (char *)tmp;

   if (resource[rid].fAddr)
   {  rc = make_addrandmask(newlink.username, &(newlink.addr), 
            &(newlink.mask));
    // Abort on invalid items addition
      if (rc < 0)
      {  syslog(LOG_ERR, "reslink_new(): New gate is invalid (%s for #%d), abort",
                newlink.username, newlink.accno);
         free(newlink.username);
         newlink.username = NULL;
         return (-1);
      }
   // Recreate inet ident string
      free(newlink.username);
      newlink.username = NULL;
      asprintf(((char**)&tmp), "%s/%d", inet_ntop(AF_INET, &(newlink.addr), addrbuf1, sizeof(addrbuf1)), mask2bits(newlink.mask));
      if (tmp == NULL)
      {  syslog(LOG_ERR, "reslink_new(calloc): %m");
         free(newlink.username);
         newlink.username = NULL;
         return (-1);
      }
      newlink.username = (char*)tmp;
        
   // Check new item for intersection (abort adding intersecting gate)
      rc = lookup_intersect(newlink.addr, newlink.mask, NULL);
      if (rc >= 0)
      {  syslog(LOG_ERR, "reslink_new(): INTERSECTION - new gate (%s for #%d) intersects old one (%s for #%d), abort",
                newlink.username, newlink.accno, linktab[rc].username, linktab[rc].accno);
         free(newlink.username);
         newlink.username = NULL;
         return (-1);
      }        
   }

   for(i = 0; i < linktabsz; i++)
      if (linktab[i].res_id == rid && linktab[i].user_id>uid) 
          uid = linktab[i].user_id;
   uid++;
   newlink.user_id = uid;
   tmp = realloc(linktab, (linktabsz+1)*sizeof(newlink));
   if (tmp == NULL)
   {  syslog(LOG_ERR, "reslink_new(calloc): %m");
      return (-1);
   }
   linktab              = (reslink_t *)tmp;
   linktab[linktabsz++] = newlink;

   return 0;
}

int reslink_del(int index)
{  void * tmp;

   if (index <0 || index>=linktabsz) return (-1);
   if (index != linktabsz-1)
      memmove(linktab+index, linktab+index+1, 
               (linktabsz-1-index)*sizeof(reslink_t));
   linktabsz--;
   if (linktabsz == 0) linktab=NULL;
   else
   {  tmp=realloc(linktab, linktabsz*sizeof(reslink_t));
      if (tmp == NULL)
         syslog(LOG_ERR, "reslink_del(realloc): %m");
      else
         linktab=(reslink_t*)tmp;
   }

   return 0;
} 


int lookup_res (int rid, int uid, int * index)
{  
   for ((*index)++; (*index)<linktabsz; (*index)++)
      if (linktab[*index].res_id==rid && linktab[*index].user_id==uid)
        return *index;
   return (-1); 
}

int lookup_resname (int rid, char * name, int * index)
{  int     rc;
   u_int   addr = 0;
   u_int   mask = 0;

// Count address value
   if (resource[rid].fAddr)
      if (make_addrandmask(name, &addr, &mask) < 0) return (-1);

   for ((*index)++; (*index)<linktabsz; (*index)++)
      if (linktab[*index].res_id == rid)
      {  if (resource[rid].fAddr) 
         {  if (linktab[*index].mask == 0) rc = 1; 
            else rc = linktab[*index].addr != (addr & linktab[*index].mask);
         }
         else rc = strcmp(name, linktab[*index].username);

         if (rc == 0) return *index;
      } 
   return (-1); 
}


int lookup_accno (int accno, int * index)
{
   for ((*index)++; (*index)<linktabsz; (*index)++)
     if (linktab[*index].accno==accno) return *index;
   return (-1);
}

int lookup_accres (int accno, int rid, int * index)
{
   for ((*index)++; (*index)<linktabsz; (*index)++)
     if (linktab[*index].accno == accno && linktab[*index].res_id == rid) return *index;
   return (-1);
}

int lookup_name (char * name, int * index)
{
   for ((*index)++; (*index)<linktabsz; (*index)++)
     if (strcasecmp(linktab[*index].username, name) == 0) return *index;
   return (-1);
}

int lookup_pname (char * name, int * index)
{
   for ((*index)++; (*index)<linktabsz; (*index)++)
#ifndef strcasestr
     if (strstr(linktab[*index].username, name) != NULL) return *index;
#else
     if (strcasestr(linktab[*index].username, name) != NULL) return *index;
#endif
   return (-1);
}

int lookup_addr (char * addr, int * index)
{  u_int   naddr = 0;
   u_int   nmask = 0;

// Count address value
   if (make_addrandmask(addr, &naddr, &nmask) < 0) return (-1);

   return lookup_baddr(naddr, index);
}

int lookup_baddr (u_int addr, int * index)
{  
   for ((*index)++; (*index)<linktabsz; (*index)++)
   {  if (linktab[*index].mask == 0) continue;
      if (linktab[*index].addr == (addr & linktab[*index].mask)) return *index;
   }

   return (-1);
}

int lookup_intersect (u_int addr, u_int mask, int * index)
{  u_int    minmask;
   int      intindex = -1;
   int    * pindex = index ? index : &intindex;
  
   for ((*pindex)++; (*pindex)<linktabsz; (*pindex)++)
   {  if (linktab[*pindex].mask == 0) continue;
      minmask = mask & linktab[*pindex].mask;
      if ((linktab[*pindex].addr & minmask) == (addr & minmask)) return *pindex;
   }

   return (-1);
}

// Compare user inet address in form n.n.n.n[/b]
// with resource link address

int inaddr_cmp(char * user, char * link)
{  u_int   uaddr;
   int     ubits;
   u_int   laddr;
   int     lbits;

   if (make_addr(link, &laddr, &lbits)==(-1)) return (-1);
   if (make_addr(user, &uaddr, &ubits)==(-1)) return (-1);
   if (ubits<lbits) return 1;   
   if (ubits>lbits) uaddr &= make_addr_mask(lbits);
   return (uaddr != laddr);  // zero if equal
}

int make_addr(const char * straddr, u_int * addr, int * bits)
{  char   buf[32];
   char * ptr=buf;
   char * str;

   strlcpy(buf, straddr, sizeof(buf));
   str=next_token(&ptr, "/");
   if (str==NULL) return (-1);
   if (inet_aton(str, (struct in_addr *)addr) != 1) return (-1);
   str=next_token(&ptr, "/");
   if (str != NULL) *bits=strtol(str, NULL, 10);
   else *bits=32;
   *addr &= make_addr_mask(*bits);
   return 0;
}

u_int make_addr_mask(int bits)
{
  if (bits < 1)   return 0;
  if (bits >= 32) return 0xffffffffUL;

  return htonl(0xffffffffUL << (32 - bits));
}   

int mask2bits(u_int mask)
{  u_int hmask;

   hmask = ntohl(mask);

   switch (hmask)
   {
      case 0x00000000: return 0;
      case 0x80000000: return 1;
      case 0xc0000000: return 2;
      case 0xe0000000: return 3;
      case 0xf0000000: return 4;
      case 0xf8000000: return 5;
      case 0xfc000000: return 6;
      case 0xfe000000: return 7;
      case 0xff000000: return 8;
      case 0xff800000: return 9;
      case 0xffc00000: return 10;
      case 0xffe00000: return 11;
      case 0xfff00000: return 12;
      case 0xfff80000: return 13;
      case 0xfffc0000: return 14;
      case 0xfffe0000: return 15;
      case 0xffff0000: return 16;
      case 0xffff8000: return 17;
      case 0xffffc000: return 18;
      case 0xffffe000: return 19;
      case 0xfffff000: return 20;
      case 0xfffff800: return 21;
      case 0xfffffc00: return 22;
      case 0xfffffe00: return 23;
      case 0xffffff00: return 24;
      case 0xffffff80: return 25;
      case 0xffffffc0: return 26;
      case 0xffffffe0: return 27;
      case 0xfffffff0: return 28;
      case 0xfffffff8: return 29;
      case 0xfffffffc: return 30;
      case 0xfffffffe: return 31;
      case 0xffffffff: return 32;
   }

   return (-1);  
}   
   
int make_addrandmask(const char * straddr, u_int * addr, u_int * mask)
{  char   buf[32];
   char * ptr = buf;
   char * str;
   char   bits;

   strlcpy(buf, straddr, sizeof(buf));
   str = next_token(&ptr, "/");
   if (str == NULL) return (-1);

   if (inet_pton(AF_INET, str, addr) != 1) return (-1);

   str = next_token(&ptr, "/");
   if (str != NULL) 
   {  bits = strtol(str, NULL, 10);
      if (bits < 0 || bits > 32) return (-1);
      *mask  = make_addr_mask(bits);

      if ((*addr & *mask) != *addr) return (-1);
   }
   else *mask = 0xffffffff;

   return 0;
}
