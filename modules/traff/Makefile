# $RuOBSD: Makefile,v 1.5 2007/09/15 11:03:31 shadow Exp $
NOMAN=
PROG=	beetraff
SRCS=	beetraff.c
BINDIR=/usr/local/bin/
CFLAGS+=-I${.CURDIR} -I../include -I../../include
COPTS+=-Wall -Werror
.include <bsd.own.mk>
.include <bsd.prog.mk>
LDADD+= -lbeeipc -lbeedb -lbee 
LDFLAGS+=-L/usr/local/lib
