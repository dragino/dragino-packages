#!/bin/sh /etc/rc.common
# Copyright (c) 2015 Edwin Chen <edwin@dragino.com>
# /etc/init.d/set-mesh-gw-mode.sh
# This script configures batman-adv correctly on startup
# It sets up bridge loop avoidance mode and mesh gateway mode on startup
START=99

start()
{
	mesh_enable=`uci get secn.mesh.mesh_enable`
	
	if [ $mesh_enable -eq 0 ];then
		logger "mesh is disable"
		exit
	fi
		logger "mesh is enable"
	# Add the adhoc interface
	/usr/sbin/batctl if add wlan0-1

	# Enable Bridge Loop Avoidance mode
	/usr/sbin/batctl bl 1
	sleep 5

	# Get the MPGW setting from /etc/config/secn
	MPGW=`uci get secn.mesh.mpgw`

	# Set up mesh gateway mode
	if   [ $MPGW = "OFF" ]; then
		/usr/sbin/batctl gw off
		uci set batman-adv.bat0.gw_mode=off
	elif [ $MPGW = "SERVER" ]; then
		/usr/sbin/batctl gw server
		uci set batman-adv.bat0.gw_mode=server
	elif [ $MPGW = "SERVER-1Mb" ]; then
		/usr/sbin/batctl gw server 1mbit
		uci set batman-adv.bat0.gw_mode='server 1mbit'
	elif [ $MPGW = "SERVER-2Mb" ]; then
		/usr/sbin/batctl gw server 2mbit
		uci set batman-adv.bat0.gw_mode='server 2mbit'
	elif [ $MPGW = "SERVER-5Mb" ]; then
		/usr/sbin/batctl gw server 5mbit
		uci set batman-adv.bat0.gw_mode='server 5mbit'
	elif [ $MPGW = "SERVER-10Mb" ]; then
		batctl gw server 10mbit
		uci set batman-adv.bat0.gw_mode='server 10mbit'
	elif [ $MPGW = "CLIENT" ]; then
		/usr/sbin/batctl gw client
		uci set batman-adv.bat0.gw_mode="client"
	fi
	
	uci commit batman-adv

	# Add bat0 to bridge now that it is configured and bl is enabled
	# Check to see if mesh is to be used on WAN side.
	WANPORT=`uci get secn.wan.wanport`
	ETHWANMODE=`uci get secn.wan.ethwanmode`
	if [ $WANPORT = "Mesh" ]; then
		brctl addif br-wan bat0
		# Force udhcpc lease renewal
		if [ $ETHWANMODE = "DHCP" ]; then	
			kill -SIGUSR1 `cat /var/run/udhcpc-br-wan.pid`
		fi
	else
		brctl addif br-lan bat0
	fi

	# Setup AP Isolation on mesh if reqd, unless it is used for WAN
	AP_ISOL=`uci get secn.accesspoint.ap_isol`
	if [ $AP_ISOL = "1" ] && [ $WANPORT != "Mesh" ]; then  
		batctl ap 1
	else
		batctl ap 0
	fi
}
