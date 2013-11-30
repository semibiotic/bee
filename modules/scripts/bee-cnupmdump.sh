#!/bin/sh
# $RuOBSD: bee-cnupmdump.sh,v 1.3 2008/11/28 04:39:29 shadow Exp $

IFACE="pflog0"

# force flush to dumpfile
kill -HUP `cat /var/cnupm/cnupm-${IFACE}.pid`

if [ $? -ne 0 ]; then
   /usr/local/bin/bee-cnupmstart.sh
   /usr/bin/logger bee-cnupmdump.sh: cnupm restarted
else
   sleep 2
fi

# output text dump for bee
/usr/local/sbin/cnupmstat -En ${IFACE}

# remove dumpfile
rm -rf /var/cnupm/cnupm-${IFACE}.dump
