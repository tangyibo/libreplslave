#!/bin/bash
#daemon 

if [ $(echo $0 | grep '^/') ]; then
    path=$(dirname $0)
else
    path=$(pwd)/$(dirname $0)
fi
cd $path

CheckProcess()
{
  if [ "$1" = "" ];
  then
    return 0
  fi
  PID="$(cat $1)"
  return `ps -e |awk '{print $1}'| grep -w ${PID}|wc -l` 
}

while [ -f "$1" ] ; do
        CheckProcess $1
        Check_RET=$?
        if [ $Check_RET -lt 1 ];
        then
            sh ./init.d restart
        fi
        sleep 1
done
