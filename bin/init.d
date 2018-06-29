#!/bin/bash
# chkconfig: 345 85 15

PROG=resp_slave_server
##################################################

. /etc/rc.d/init.d/functions

if [ -z "$(readlink $0)" ]; then
    SCRIPT_DIR="$(cd $(dirname $0); pwd)"
else
    SCRIPT_DIR="$(cd $(dirname $(readlink $0)); pwd)"
fi

RUN_USER=root
PROG_HOME=${SCRIPT_DIR}
EXEC=${PROG_HOME}/restart.sh
DAEMON=${PROG_HOME}/daemon.sh
PID_FILE=${PROG_HOME}/run/${PROG}.pid
PID_FILE_BAK=${PROG_HOME}/run/${PROG}bak.pid
LOCK_FILE=/var/lock/subsys/${PROG}
RETVAL=0
echo ${PROG_HOME}/${PROG}

if [ ! -x ${EXEC} ]; then
    exit 5
fi

start() {
    action "Starting $PROG: " daemon --user=$RUN_USER --pidfile $PID_FILE $EXEC
    RETVAL=$?
    [ $RETVAL -eq 0 ] && touch $LOCK_FILE
    return $RETVAL
}

stop() {
    mv $PID_FILE $PID_FILE_BAK
    action "Stopping $PROG: " killproc -p $PID_FILE_BAK $EXEC
    RETVAL=$?
    [ $RETVAL -eq 0 ] && rm -f $LOCK_FILE
    return $RETVAL
}

restart() {
    stop
    start
}

rhstatus() {
    status -p $PID_FILE -l $LOCK_FILE $PROG
}

rhstatus_q() {
    rhstatus >/dev/null 2>&1
}

case $1 in
    start)
        rhstatus_q && exit 0
        start
        COMMAND="${DAEMON} ${PID_FILE}"
        nohup ${COMMAND} >>/dev/null 2>&1 & 
        ;;
    stop)
        rhstatus_q || exit 0
        stop
        ;;
    restart)
        restart
        ;;
    status)
        rhstatus
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}" 
        exit 3
        ;;
esac

exit $?
