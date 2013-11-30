# $RuOBSD: Makefile,v 1.12 2008/01/29 02:24:37 shadow Exp $

SCRIPTS = beeapply.sh newlogin.sh beetraff-ipstatd.sh beepfrules.sh beetraff-cnupm.sh bee-cnupmdump.sh bee-cnupmstart.sh
SBIN    = autoreg.sh intractl.sh

clean:
all:
install: 
	@echo ">>> Installing scripts  to /usr/local/bin <<<"
	@for entry in ${SCRIPTS}; do \
		(echo ">>> $${entry}"; \
                 install -c -o root -g bin -m 550 $${entry} \
			/usr/local/bin/); \
        done;
	@echo ">>> Installing scripts to /usr/local/sbin <<<"
	@for entry in ${SBIN}; do \
		(echo ">>> $${entry}"; \
                 install -c -o root -g bin -m 550 $${entry} \
			/usr/local/sbin/); \
        done;

