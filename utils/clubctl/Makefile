# $RuOBSD: Makefile,v 1.1 2002/08/09 03:24:31 shadow Exp $
NOMAN=
PROG=	clubctl
SRCS=	clubctl.c 
BINDIR=/usr/local/bin
CFLAGS+=-I${.CURDIR} -I../../include
COPTS+=-Wall -Werror
.include <bsd.own.mk>
.include <bsd.prog.mk>
LDADD+= -lbeeipc -lbee
LDFLAGS+=-L/usr/local/lib
