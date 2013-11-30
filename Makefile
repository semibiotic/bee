# $RuOBSD: Makefile,v 1.3 2004/05/08 17:21:02 shadow Exp $


BINDIR = /usr/local/bin/bee
WORKDIR= /var/bee
SUBDIR=	core modules rulers utils
.include <bsd.subdir.mk>

libs:
	@echo ">>> Making bee libraries <<<"
	@cd lib && make clean all
	@echo ">>> Installing bee libraries <<<"
	@cd lib && make install

#        if ! (test -d ${BINDIR}) then \ 
#		echo ">>> Creating binaries dir <<<"
#        	mkdir ${BINDIR} && \
#        	chown root:wheel ${BINDIR} && \
#        	chmod 0755 ${BINDIR}\
#	fi;
#	if ! (test -d ${WORKDIR}) then \ 
#        	{@echo ">>> Creating working dir <<<" \
#        	mkdir ${WORKDIR} && \
#       	chown root:wheel ${WORKDIR} && \
#        	chmod 0755 ${WORKDIR} } \
#	fi;
     



