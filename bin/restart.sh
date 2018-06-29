#!/bin/bash
#start program

if [ -z "$(readlink $0)" ]; then
    SCRIPT_DIR="$(cd $(dirname $0); pwd)"
else
    SCRIPT_DIR="$(cd $(dirname $(readlink $0)); pwd)"
fi

################################################################
APP_HOME=${SCRIPT_DIR}
APP_NAME='resp_slave_server'
PID_FILE="${APP_HOME}/run/${APP_NAME}.pid"
RUN_LOG="${APP_HOME}/run/run_${APP_NAME}.log"

[ -d "${APP_HOME}/run" ] || mkdir -p "${APP_HOME}/run"
cd ${APP_HOME}

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64:./lib
ulimit -c unlimited

echo -n `date +'%Y-%m-%d %H:%M:%S'`              >>${RUN_LOG}
echo "---- Start service [${APP_NAME}] process. ">>${RUN_LOG}

nohup ${APP_HOME}/${APP_NAME} >>${RUN_LOG} 2>&1 &

################################################################

RETVAL=$?
PID=$!

echo ${PID} >${PID_FILE}
exit ${RETVAL}
