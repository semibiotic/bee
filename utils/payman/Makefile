# $RuOBSD: Makefile,v 1.14 2008/08/20 07:49:04 shadow Exp $
NOMAN=
PROG=	payman
SRCS=	dialog.cc dispatch.cc display.cc      \
        init.cc keybar.cc menu.cc menubar.cc msgbox.cc   \
        panel.cc payman.cc unicon.cc waiter.cc winout.cc \
        list.cc title.cc userview.cc help.cc       \
        login.cc log.cc inetpay.cc beetalk.cc intrapay.cc

BINDIR=/usr/local/bin/
CFLAGS+= -I../../include
COPTS+=-g -Wall

.include <bsd.own.mk>
.include <bsd.prog.mk>

LDADD+= -lcurses -lbeedb -lbeeipc -lz -lbee
LDFLAGS+=-L/usr/local/lib


