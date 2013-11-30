# $RuOBSD: Makefile,v 1.3 2002/06/02 13:20:28 shadow Exp $
NOMAN=
PROG=	beeipf
SRCS=	beeipf.c
BINDIR=/usr/local/bin/
CFLAGS+=-I${.CURDIR} -I../../include
COPTS+=-Wall -Werror
.include <bsd.own.mk>
.include <bsd.prog.mk>
LDADD+= -lbee
LDFLAGS+=-L/usr/local/lib
