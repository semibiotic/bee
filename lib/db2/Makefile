# $RuOBSD: Makefile,v 1.1 2007/09/19 04:49:20 shadow Exp $

SRCS=  db.c
LIB=   bee2db
LIBDIR= /usr/local/lib
CFLAGS+=-I${.CURDIR} -I../../include -I/usr/local/include
COPTS+=	-Wall -Werror 
NOPROFILE=
LDFLAGS+=-L/usr/local/lib
#LDADD+= 
.include <bsd.lib.mk>
.include <bsd.own.mk>
