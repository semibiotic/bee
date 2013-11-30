# $RuOBSD: Makefile,v 1.1 2010/11/13 22:34:45 shadow Exp $
NOMAN=
PROG=	beemkacl_c3745
SRCS=	mkacl.c
BINDIR=/usr/local/bin/
CFLAGS+=-I${.CURDIR} -I../../include
COPTS+=-Wall -Werror
.include <bsd.own.mk>
.include <bsd.prog.mk>
LDADD+= -lbeedb -lbee
LDFLAGS+=-L/usr/local/lib
