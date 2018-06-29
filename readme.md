librespslave
=============
 接收mysql-binlog日志并解析后推送至kafka。
------------------------------------------
一、环境
 
 yum -y install openssl openssl-devel

 yum -y install boost boost-devel

二、编译
 
 cd libreplslave/

 make clean

 make all

三、运行
 
 cd libreplslave/bin
  
 ./restart.sh
