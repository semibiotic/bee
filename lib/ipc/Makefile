# $RuOBSD: Makefile,v 1.3 2002/01/14 08:44:48 shadow Exp $
PROG=  bee
SRCS=  ipc.c
LIB=   beeipc
LIBDIR= /usr/local/lib
CFLAGS+=-I${.CURDIR} -I../../include -I/usr/local/include
COPTS+=	-Wall -Werror 
NOPROFILE=
LDFLAGS+=-L/usr/local/lib
LDADD+= -lbee
.include <bsd.lib.mk>
.include <bsd.own.mk>
