#!/bin/sh
#A script to update selected packages. 
## Copyright (c) 2015 Dragino Tech <support@dragino.com>

PKG_PID=`ps | grep "update_packages" | grep -v grep | awk '{print $1}'`
SELF_PID=$$
if [ ! -z "$PKG_PID" ];then
	for pid in $PKG_PID;do 
		if [ $pid != $SELF_PID ]; then
			kill -s 9 $pid
		fi 
	done
fi

CHECK_HOST=`uci get auto_update_pkg.general.chk_host`
if [ ! -z $CHECK_HOST ]; then
	#Check_host defined. process update only when there is network connection to Check Host.
	MAX_WARN=5
	CUR_WARN=1
	while [ -z "`fping -e $CHECK_HOST | grep alive`" ]
	do
		if [ $CUR_WARN -le $MAX_WARN ];then
			logger '[Auto Update]: No net connection to check host'
			CUR_WARN=`expr $CUR_WARN + 1`
		fi
		#
		sleep 10
	done
	
fi

logger '[Auto Update]: Connect to remote server for up-to-date pacakges.'

OPKG_CONF=`uci get auto_update_pkg.general.opkg_file`
SEL_PKGS=`uci get auto_update_pkg.general.selected_packages`

opkg update --conf $OPKG_CONF
opkg upgrade --conf $OPKG_CONF $SEL_PKGS