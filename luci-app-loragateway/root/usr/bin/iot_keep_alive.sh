#!/bin/sh

DEVPATH=/sys/bus/usb/devices
#VID=12d1
#PID=1001 
WAN_IF='eth1' # enter name of WAN interface
WANCARRIER=/sys/class/net/${WAN_IF}/carrier
WIFI_IF='wlan0'
#WIFICARRIER=/sys/class/net/${WIFI_IF}/carrier
WIFI_GW=""
wifi_ip=""

#GSM_GW='10.64.64.64' # enter IP for default gateway on 3G interface
#GSM_NAME=yun3G #enter name for 3G
GSM_IF=`ifconfig |grep "3g-" | awk '{print $1}'` # 3g interface
PING_HOST='1.1.1.1' # enter IP of host that you want to check,
gsm_mode=0
toggle_3g_time=90
last_check_3g_dial=0
ONE="1"
ZERO="0"
iot_online=0
default_gw_3g=0
offline_flag=""
is_lps8=`hexdump -v -e '11/1 "%_p"' -s $((0x908)) -n 11 /dev/mtd6 | grep -c lps8`
last_reload_time=`date +%s`

board=`cat /var/iot/board`
if [ "$board" = "LG01" ] || [ "$board" = "LG02" ];then
	Cellular_CTL=1
else
	Cellular_CTL=15
fi

echo $Cellular_CTL > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio$Cellular_CTL/direction

update_state()
{
    #echo "lan"
	global_ping="$ZERO"
	global_ping=`fping $PING_HOST | grep -c alive`
	
	wan_ping="$ZERO"
    #wan_chk=`ifconfig |grep "$WAN_IF"`
    WAN_GW=`ip route show table all | grep "$WAN_IF" | grep default | awk '{print $3;}'` # get IP for default gateway on WAN interface
    wan_link=`cat $WANCARRIER`
    wan_ip=`ifconfig "$WAN_IF" | grep "inet " | awk -F'[: ]+' '{ print $4 }'`
    logger -t iot_keep_alive "Ping WAN $wan_ip"
    [ -n "$wan_ip" ] && wan_ping=`fping -I $WAN_IF $PING_HOST | grep -c alive`
	[ "$wan_ping" -gt "$ZERO" ] && logger -t iot_keep_alive "WAN is alive"
    #echo "lan-end"
	
    wifi_ping="$ZERO"

    #wifi_chk=`ifconfig |grep "$WIFI_IF"`
	if [ `ifconfig | grep wlan0 -c` -gt 0 ];then 
		#echo "wifi"
		WIFI_GW=`ip route show table all | grep "$WIFI_IF" | grep default | awk '{print $3;}'` # get IP for default gateway on WIFI interface
		wifi_ip=`ifconfig "$WIFI_IF" | grep "inet " | awk -F'[: ]+' '{ print $4 }'`
		#wifi_disabled=`uci get wireless.@wifi-iface[0].disabled`
		#wifi_ap_mode=`uci get wireless.@wifi-iface[0].mode`
		#wifi_link=`cat $WIFICARRIER`
		logger -t iot_keep_alive "Ping WIFI $wifi_ip"
		#[ -n "$wifi_ip" ] && [ "$wifi_ap_mode" != "ap" ] && wifi_ping=`ping -q -4 -c 3 -I $wifi_ip $PING_HOST | grep received |awk '{print $4}'`
		[ -n "$wifi_ip" ] && wifi_ping=`fping -I $WIFI_IF $PING_HOST | grep -c alive`
		[ "$wifi_ping" -gt "$ZERO" ] && logger -t iot_keep_alive "WiFi is alive"
		#echo "wifi-end"
	fi 

	
	
    default_route_if=`ip route show | grep default | awk '{print $5}'`
	logger -t iot_keep_alive "Default interface is $default_route_if"

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
	logger -t iot_keep_alive "gsm_ping: $gsm_ping"
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

        [ -n "$previous_gw" ] && ip route del $previous_gw
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
    update_state
	[ ! -z $GSM_IF ] && default_gw_3g=`ip route show | grep default | grep $GSM_IF -c`
	has_internet=0	
	
	if [ "$global_ping" -gt "$ZERO" ];then   # Check if the device has internet connection
		logger -t iot_keep_alive "Global Internet Access OK"
		has_internet=1	
		if [ "$wan_ping" -gt "$ZERO" ] || [ "$wifi_ping" -gt "$ZERO" ];then  #Check If device has WIFi or WAN Connection.
			logger -t iot_keep_alive "use WAN or WiFi for internet access now"
			if [ "$default_gw_3g" == "1" ];then
				logger -t iot_keep_alive "need to switch to use ETH or WLAN"  # Device has WiFi or WAN connection but now use 3G as default gateway, Change back to WAN or WiFi
				if [ "$wan_ping" -gt "$ZERO" ];then
					use_wan_as_gateway
				else
					use_wifi_as_gateway
				fi
			fi
		fi
	else
		if [ "$wan_ping" -gt "$ZERO" ]; then
			has_internet=1	
			use_wan_as_gateway
		elif [ "$wifi_ping" -gt "$ZERO" ]; then
			has_internet=1
			use_wifi_as_gateway
		else
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
		[ "$offline_flag" = "0" ] && echo "`date`: switch to offline" >> /var/status_log
		offline_flag="1"
		reload_iot_service
	else 
		# IoT Connection Fail, Internet Down
		echo 0 > /sys/class/leds/dragino2\:red\:system/brightness
		[ "$is_lps8" = "1" ] && echo 1 > /sys/class/gpio/gpio21/value
	fi
	#echo "iot-end"	
	
    #if [ -z "$WAN_GW" ] && [ "$wan_link" -eq "$ONE" ]; then
      #  update_gateway $WAN_IF
     #   logger -t iot_keep_alive "no wan gateway, retry to get gateway"
    #fi
	
	#if [ -n "$wifi_ip" ] && [ "$wifi_ping" -eq "$ZERO" ]; then
    #if [ "$wifi_disabled" -eq "$ZERO" ] && [ "$wifi_ap_mode" != "ap" ] && [ "$wifi_link" -eq "$ONE" ] && [ -z "$WIFI_GW" ]; then	
     #   update_gateway $WIFI_IF
      #  logger -t iot_keep_alive "has wifi gateway, but no internet connection, reset WiFi DHCP"
	#	echo "reset wifi"
    #fi
done
