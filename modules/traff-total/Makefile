# $RuOBSD: Makefile,v 1.2 2007/09/25 14:49:01 shadow Exp $
NOMAN=
PROG=	beetraff-tot
SRCS=	beetraff.c
BINDIR=/usr/local/bin/
CFLAGS+=-I${.CURDIR} -I../include -I../../include
COPTS+=-Wall -Werror
.include <bsd.own.mk>
.include <bsd.prog.mk>
LDADD+= -lbeeipc -lbeedb -lbee
LDFLAGS+=-L/usr/local/lib
