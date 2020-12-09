#!/bin/sh

DEVPATH=/sys/bus/usb/devices
#VID=12d1
#PID=1001 
WAN_IF='eth1' # enter name of WAN interface
WAN_GW=""
RETRY_WAN_GW=0

#WiFi Interface
WIFI_IF='wlan0-2'
WIFI_GW=""
RETRY_WIFI_GW=0
wifi_ip=""


GSM_IF="3g-cellular" # 3g interface

PING_HOST='1.1.1.1' # enter IP of first host to check.
PING_HOST2='8.8.8.8' # enter IP of second host to check.
PING_WIFI_HOST="8.8.4.4" # Use this IP to check WiFi Connection. 
PING_WAN_HOST="139.130.4.5"  # Use this IP to check WAN connection (ns1.telstra.net)

gsm_mode=0
toggle_3g_time=90
last_check_3g_dial=0
ONE="1"
ZERO="0"
iot_online=0
offline_flag=""
is_lps8=`hexdump -v -e '11/1 "%_p"' -s $((0x908)) -n 11 /dev/mtd6 | grep -c -E "lps8|los8|ig16"`
last_reload_time=`date +%s`

board=`cat /var/iot/board`
if [ "$board" = "LG01" ] || [ "$board" = "LG02" ];then
	Cellular_CTL=1
else
	Cellular_CTL=15
fi

echo $Cellular_CTL > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio$Cellular_CTL/direction



chk_internet_connection()
{
	global_ping="$ZERO"
	global_ping=`fping $PING_HOST | grep -c alive`
	if [ "$global_ping" -eq "$ZERO" ];then
		global_ping=`fping $PING_HOST2 | grep -c alive`
	fi
}

chk_eth1_connection()
{
	wan_ping="$ZERO"
	
	if [ `ifconfig | grep $WAN_IF -c` -gt 0 ];then
		wan_ip="`ifconfig "$WAN_IF" | grep "inet " | awk -F'[: ]+' '{ print $4 }'`"
		

		if [ -n "$wan_ip" ]; then
			#Try to get WAN GW 
			if [ -z $WAN_GW ] && [ "`uci get network.wan.proto`" = "dhcp" ] && [ $RETRY_WAN_GW -lt 5 ];then
				ifup wan
				RETRY_WAN_GW=`expr $RETRY_WAN_GW + 1`
				logger -t iot_keep_alive "Retry $RETRY_WAN_GW to get wan gateway"
				sleep 20
				WAN_GW=`ip route | grep "$WAN_IF" | grep default | awk '{print $3;}'`	
				[ -n $WAN_GW ] && ip route add $PING_WAN_HOST via $WAN_GW dev $WAN_IF
			fi
			
			# Ping Host to check eth1 connection. 
			if [ "`ip route | grep $PING_WAN_HOST | awk '{print $3;}'`" = "$WAN_GW"  ];then
				logger -t iot_keep_alive "Ping WAN via $WAN_GW"
				wan_ping=`fping $PING_WAN_HOST | grep -c alive`
			fi 
		fi
	fi
}

chk_wlan0_connection()
{
    wifi_ping="$ZERO"

	if [ `ifconfig | grep $WIFI_IF -c` -gt 0 ];then 
		wifi_ip="`ifconfig "$WIFI_IF" | grep "inet " | awk -F'[: ]+' '{ print $4 }'`"
		

		if [ -n "$wifi_ip" ]; then
			#Try to get WiFi GW 
			if [ -z $WIFI_GW ] && [ "`uci get network.wwan.proto`" = "dhcp" ] && [ $RETRY_WIFI_GW -lt 5 ];then
				ifup wwan
				RETRY_WIFI_GW=`expr $RETRY_WIFI_GW + 1`
				logger -t iot_keep_alive "Retry $RETRY_WIFI_GW to get wifi GW"
				sleep 20
				WIFI_GW=`ip route | grep "$WIFI_IF" | grep default | awk '{print $3;}'` # get IP for default gateway on WIFI interface	
				[ -n $WIFI_GW ] && ip route add $PING_WIFI_HOST via $WIFI_GW dev $WIFI_IF
			fi
			
			# Ping  Host to check wifi connection. 
			if [ "`ip route | grep $PING_WIFI_HOST | awk '{print $3;}'`" = "$WIFI_GW"  ];then
				logger -t iot_keep_alive "Ping WiFi via $WIFI_GW"
				wifi_ping=`fping $PING_WIFI_HOST | grep -c alive`
			fi 
		fi
	fi 	
}

reload_iot_service()
{
		cur_reload_time=`date +%s`
		if [ "`uci get gateway.general.server_type`" = "lorawan" ] && [ `expr $cur_reload_time - $last_reload_time` -gt 40 ];then
			#Socket Reconnect
			ps | grep "pkt_fwd" | grep -v grep | awk '{print $1}' | xargs kill -USR1
			last_reload_time=`date +%s`
		fi	
}

check_3g_connection()
{
    #echo "3g"
	GSM_IF=`ifconfig |grep "3g-" | awk '{print $1}'` # 3g interface
	gsm_ping="$ZERO"
	[ -z $GSM_IF ] && return
    #modem_chk=`lsusb |grep ${VID}:${PID}`
    gsm_chk=`ifconfig |grep "$GSM_IF"`
    gsm_ip=`ifconfig | grep '3g-' -A 1 | grep 'inet' | awk -F'[: ]+' '{ print $4 }'`
    [ -n "$gsm_ip" ] && logger -t iot_keep_alive "Ping GSM $gsm_ip" && gsm_ping=`fping -I $GSM_IF $PING_HOST | grep -c alive`
	if [ "$gsm_ping" -eq "$ZERO" ];then
		gsm_ping=`fping -I $GSM_IF $PING_HOST2 | grep -c alive`
	fi
	[ "$gsm_ping" -gt "$ZERO" ] && logger -t iot_keep_alive "GSM Cellular is alive"
	GSM_GW=`ip route show| grep $GSM_IF | awk '{print $1}'`
	#echo "3g-end"
}

gsm3g_connect()
{
        # this function connects modem to 3G network -
        # usually used when we lose internet connection on WAN interface and failover occurs
        # or when 3G connection is disconnected by operator (example: Aero2 operator in Poland)
        logger -t iot_keep_alive "Connecting to 3G network..."
        ifup $GSM_NAME
        echo 1 > /root/gsm_mode
        sleep 15
}
 
gsm3g_disconnect()
{
        # this function disconnects modem from 3G network -
        # usually used when we recover internet connection on WAN interface
        logger -t iot_keep_alive "Disconnecting from 3G network..."
        ifdown $GSM_NAME
        echo 0 > /root/gsm_mode
        sleep 5
}
 
use_gsm_as_gateway()
{
        # this function add default gateway for 3G interface -
        # usually executed when moving internet connection from WAN to 3G interface
        logger -t iot_keep_alive "Moving internet connection to $GSM_IF (3G) via gateway $GSM_GW..."
      #  previous_gw=`ip route show | grep default | awk '{print $3}'`
      #  [ -n "$previous_gw" ] && ip route del $previous_gw
        ip route del default

        ip route add default via $GSM_GW dev $GSM_IF proto static
        #ip route add $GSM_GW dev $GSM_IF proto static scope link src $gsm_ip
}
 
use_wan_as_gateway()
{
        # this function remove default gateway for 3G interface -
        # usually executed when moving internet connection from 3G to WAN interface
        logger -t iot_keep_alive "Moving internet connection to $WAN_IF (WAN) via gateway $WAN_GW..."

        ip route del default
        ip route add default via $WAN_GW dev $WAN_IF proto static src $wan_ip
        #ip route add $WAN_GW dev $WAN_IF proto static scope link src $wan_ip

}

use_wifi_as_gateway()
{
        # this function remove default gateway for 3G interface -
        # usually executed when moving internet connection from 3G to WIFI interface
        logger -t iot_keep_alive "Moving internet connection to $WIFI_IF (WIFI) via gateway $WIFI_GW..."		
        ip route del default
        ip route add default via $WIFI_GW dev $WIFI_IF proto static src $wifi_ip
        #ip route add $WIFI_GW dev $WIFI_IF proto static scope link src $wifi_ip
}

gsm_poweroff()
{
    logger -t iot_keep_alive "Turning off GSM module"
    echo 0 > /sys/devices/virtual/gpio/gpio$Cellular_CTL/value
}

gsm_poweron()
{
    logger -t iot_keep_alive "Turning on GSM module"
    echo 1 > /sys/devices/virtual/gpio/gpio$Cellular_CTL/value
}

update_gateway()
{
    pid=$(cat "/var/run/udhcpc-$1.pid")
    kill -s SIGUSR2 "$pid" #release lease
    sleep 5
    kill -s SIGUSR1 "$pid" #get new lease
    sleep 5
}

toggle_3g()
{
    GSM_IF_SUF=`ifconfig |grep '3g-' | awk '{print $1}' | awk -F '-' '{print $2}'`
	ifdown $GSM_IF_SUF
    sleep 240
    ifup $GSM_IF_SUF
    logger -t iot_keep_alive "Restart 3G Interface"
}

while :
do 
	sleep 15
	chk_internet_connection
	
	# Control receive size < 2M
	receive_size=`du -h -k /var/iot/ -d 0 | awk '{print $1}'`
	if [ $receive_size -gt 1024 ];then
		rm -rf /var/iot/receive/*
		rm -rf /var/iot/channels/*
	fi
	
	if [ "$global_ping" -gt "$ZERO" ];then   # Check if the device has internet connection
		ROUTE_DF=`ip route | grep default | awk '{print $5}'`
		logger -t iot_keep_alive "Internet Access OK: via $ROUTE_DF"
		has_internet=1	
		if [ "$ROUTE_DF" = "$WAN_IF" ] || [ "$ROUTE_DF" = "$WIFI_IF" ];then  #Check If device has WIFi or WAN Connection.
			logger -t iot_keep_alive "use WAN or WiFi for internet access now"
		elif [ "$ROUTE_DF" = "$GSM_IF" ] && [ "`uci get network.cellular.backup`" = "1" ];then
			chk_eth1_connection
			if [ "$wan_ping" -gt "$ZERO" ];then
				use_wan_as_gateway
			else 
				logger -t iot_keep_alive "XX ping ETH1 $PING_WAN_HOST via eth1 fail"
				chk_wlan0_connection
				if [ "$wifi_ping" -gt "$ZERO" ];then
					use_wifi_as_gateway
				else
					logger -t iot_keep_alive "XX ping WiFi $PING_WIFI_HOST via wlan0-2 fail"
				fi
			fi
		fi
	else
		has_internet=0
		logger -t iot_keep_alive "Internet fail. Check interfaces for network connection"
		chk_eth1_connection
		if [ "$wan_ping" -gt "$ZERO" ];then
			use_wan_as_gateway
			has_internet=1
		else 
			logger -t iot_keep_alive "XX ping ETH1 $PING_WAN_HOST via eth1 fail"
			chk_wlan0_connection
			if [ "$wifi_ping" -gt "$ZERO" ];then
				use_wifi_as_gateway
				has_internet=1
			else
				logger -t iot_keep_alive "XX ping WiFi $PING_WIFI_HOST via wlan0-2 fail"
				check_3g_connection
				if [ "$gsm_ping" -gt "$ZERO" ]; then
					has_internet=1
					use_gsm_as_gateway
				else
				#All Interface doesn't have internet connection, reset all. 
					logger -t iot_keep_alive "No internet at any interface"
					update_gateway $WAN_IF
					update_gateway $WIFI_IF
				fi 
			fi
		fi
	fi	

	#Show LED status
	# echo 1 > /sys/class/leds/dragino2\:red\:system/brightness GPIO28
	# LPS8:GPIO21: RED, GPIO28: Blue GLobal
	# LGxx: GPIO21: N/A. GPIO28: RED Global
	# Check IoT Connection first. 
	#echo "iot"
	if [ "$is_lps8" = "1" ];then
		if [ "`ls /sys/class/gpio/ | grep -c gpio21`" = "0" ];then
			echo 21 > /sys/class/gpio/export
			echo out > /sys/class/gpio/gpio21/direction
		fi
	fi
	
	if [ "`ps | grep "/usr/bin/fwd" -c`" == 2 ];then
		status=`sqlite3 /var/lgwdb.sqlite "select * from gwdb where key like '/service/lorawan/server/network';" | grep -c online`
		if [ "$status" == "1" ];then
			echo "online" > /var/iot/status
		else 
			echo "offline" > /var/iot/status
		fi 
	fi
	iot_online=`cat /var/iot/status | grep online -c`	

	
	/usr/bin/blink-stop
	if [ "$iot_online" = "1" ]; then
		# IoT Connection is ok
		[ "$is_lps8" = "1" ] && echo 0 > /sys/class/gpio/gpio21/value
		echo 1 > /sys/class/leds/dragino2\:red\:system/brightness
		[ "$offline_flag" = "1" ] && offline_flag="0" && echo "`date`: switch to online" >> /var/status_log
	elif [ $has_internet -eq 1 ]; then
		# IoT Connection Fail, but Internet Up
		/usr/bin/blink-start 100   #GPIO28 blink, periodically: 200ms
		[ "$is_lps8" = "1" ] && echo 0 > /sys/class/gpio/gpio21/value
		if [ "$offline_flag" = "0" ]; then
			echo "`date`: switch to offline" >> /var/status_log
			offline_flag="1"
		elif [ "$offline_flag" = "1" ]; then
			logger -t iot_keep_alive "Reload IoT Service"
			reload_iot_service
		fi
	else 
		# IoT Connection Fail, Internet Down
		echo 0 > /sys/class/leds/dragino2\:red\:system/brightness
		[ "$is_lps8" = "1" ] && echo 1 > /sys/class/gpio/gpio21/value
	fi
	#echo "iot-end"	
done
