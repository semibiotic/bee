#!/bin/sh

LOG_DIR=/var/bee
LOG_FILE=${LOG_DIR}/beelog.dat
IDX_FILE=${LOG_DIR}/beelog.idx

PART1_FILE=${LOG_DIR}/beelog.1
PART2_FILE=${LOG_DIR}/beelog.2
BAK_FILE=${LOG_DIR}/beelog.bak

SPLIT_TIME=$1

if [ -z "${SPLIT_TIME}" ]; then
    echo "USAGE: $0 <DD.MM.YYYY>"
    exit 1
fi

if [ -f "${BAK_FILE}" ]; then
   echo "${BAK_FILE} - file exists !"
   exit 1
fi

echo -n "Splitting log ... "

beelog-split -F ${SPLIT_TIME} -o ${PART1_FILE},${PART2_FILE} -f ${LOG_FILE} -i ${IDX_FILE}

RESULT=$?
 
if [ "${RESULT}" != "0" ]; then
    echo "FAILURE."
    exit 1
fi

echo "done."
echo -n "Replacing log ... "

mv ${LOG_FILE}   ${BAK_FILE}
mv ${PART2_FILE} ${LOG_FILE}

echo "done."
echo -n "Reindexing ... "

beelogidx 

echo "done."
