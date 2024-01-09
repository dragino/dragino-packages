#!/bin/sh


#History:
# 2021/1/6   Add function station_check_time
#			 Add WAN and WWAN are get the gateway at staic 
# 2021/1/20  Add AT&T disconnection detection
# 2022/10/18 Add gsm check

DEVPATH="/sys/bus/usb/devices"
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
iot_interval=`uci get system.@system[0].iot_interval`


GSM_IF="3g-cellular" # 3g interface
GSM_IMSI=""
ENABLE_USAGE="0"

PING_HOST='1.1.1.1' # enter IP of first host to check.
PING_HOST2='8.8.8.8' # enter IP of second host to check.
PING_WIFI_HOST="8.8.4.4" # Use this IP to check WiFi Connection. 
PING_WAN_HOST="139.130.4.5"  # Use this IP to check WAN connection (ns1.telstra.net)

check_gsm=0
gsm_poweroff_time=""
gsm_mode=0
gsm_enable=`uci get network.cellular.auto`
toggle_3g_time=90
last_check_3g_dial=0
RETRY_POWEROFF_GSM=5
RETRY_REBOOT_GSM=61
ONE="1"
ZERO="0"
retry_gsm=0
iot_online="1"
offline_flag="1"
is_lps8=`hexdump -v -e '11/1 "%_p"' -s $((0x908)) -n 11 /dev/mtd6 | grep -c -E "lps8|los8|ig16|ps8n|ps8g|os8n|os8l|ps8l"`

#last_reload_time=`date +%s`
station_check_time=1

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
	echo "$global_ping" > /var/iot/internet                                                                       
}

chk_eth1_connection()
{
	wan_ping="$ZERO"

	if [ `ifconfig | grep $WAN_IF -c` -gt 0 ];then
		wan_ip="`ifconfig "$WAN_IF" | grep "inet " | awk -F'[: ]+' '{ print $4 }'`"
		if [ -n "$wan_ip" ]; then
			#Try to get WAN GW 
			proto=`uci get network.wan.proto`
			if [ -z $WAN_GW ] && [ "`uci get network.wan.proto`" = "dhcp" ] && [ $RETRY_WAN_GW -lt 5 ];then
				ifup wan
				RETRY_WAN_GW=`expr $RETRY_WAN_GW + 1`
				logger -t iot_keep_alive "Retry $RETRY_WAN_GW to get wan gateway"
				sleep 20
				WAN_GW=`ip route | grep "$WAN_IF" | grep default | awk '{print $3;}'`	
				[ -n $WAN_GW ] && ip route add $PING_WAN_HOST via $WAN_GW dev $WAN_IF
			elif [ -z $WAN_GW ] && [ "`uci get network.wan.proto`" = "static" ];then
				WAN_GW=`uci get network.wan.gateway`
				[ -n $WAN_GW ] && ip route add $PING_WAN_HOST via $WAN_GW dev $WAN_IF
				logger -t iot_keep_alive "$WAN_GW"
			fi
			
			# Ping Host to check eth1 connection. 
			if [ "`ip route | grep $PING_WAN_HOST | awk '{print $3;}'`" = "$WAN_GW"  ];then
				logger -t iot_keep_alive "Ping WAN via $WAN_GW"
				wan_ping=`fping $PING_WAN_HOST | grep -c alive`
			elif [ "`uci get network.wan.proto`" = "static" ]; then
				wan_ping=`fping $WAN_GW | grep -c alive`
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
			
				
			elif [ -z $WIFI_GW ] && [ "`uci get network.wwan.proto`" = "static" ] && [ $RETRY_WIFI_GW -lt 5 ];then
				WIFI_GW='uci get network.wwan.gateway'
				[ -n $WIFI_GW ] && ip route add $PING_WIFI_HOST via $WIFI_GW dev $WIFI_IF
			fi
			
			# Ping  Host to check wifi connection. 
			if [ "`ip route | grep $PING_WIFI_HOST | awk '{print $3;}'`" = "$WIFI_GW"  ];then
				logger -t iot_keep_alive "Ping WiFi via $WIFI_GW"
				wifi_ping=`fping $PING_WIFI_HOST | grep -c alive`
			elif [ "`uci get network.wwan.proto`" = "static" ]; then
				wifi_ping= `fping $WIFI_GW | grep -c alive`
			fi 
		fi
	fi 	
}

reload_iot_service()
{
		cur_reload_time=`date +%s`
		if [ "`uci get gateway.general.server_type`" = "lorawan" ];then
			#Socket Reconnect
			#ps | grep "fwd" | grep -v grep | awk '{print $1}' | xargs kill -USR1
			/etc/init.d/lora_gw reload
		elif [ "`uci get gateway.general.server_type`" = "station" ] && [ `expr $cur_reload_time - $last_reload_time` -gt 90 ];then
			/usr/bin/reload_iot_service.sh &
			last_reload_time=`date +%s`
		fi	
}

check_3g_connection()
{

	if [ -z "$GSM_IMSI" ]; then # Get Cellular GSM_IMSI
		killall comgt;
		GSM_IMSI='gcom -d /dev/ttyModemAT -s /etc/gcom/getimsi.gcom | cut -c 1,2,3,4,5,6'
		if [ -z "$(echo $GSM_IMSI | sed -n "/^[0-9]\+$/p")" ];then 
			GSM_IMSI=""
		fi
	fi

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
    echo 1 > /sys/class/gpio/gpio$Cellular_CTL/value
}

gsm_poweron()
{
    logger -t iot_keep_alive "Turning on GSM module"
    echo 0 > /sys/class/gpio/gpio$Cellular_CTL/value
}

update_gateway()
{
    pid=$(cat "/var/run/udhcpc-$1.pid")
    kill -s SIGUSR2 "$pid" #release lease
    sleep 5
    kill -s SIGUSR1 "$pid" #get new lease
    sleep 5
}

gsm_status_check()
{
	local current_time
	local disable_time
	local gsm_enable
	gsm_enable=$(uci -q get network.cellular.auto)
	if [ "$gsm_enable" == "0" ]; then
		return 0
	fi

	if [ "$(cat /sys/class/gpio/gpio$Cellular_CTL/value)" == "0" ]; then
		gsm_hangup=$(logread |grep -e "Modem hangup" -c)
		if [ "$gsm_hangup" -ge "6" ]; then
			#uci set network.cellular.auto=0
			logger -t iot_keep_alive "Cellular network is not working properly"
			echo "Cellular multiple dialing failures detected, But the gateway can be accessed through other interfaces, so $(date) cellular is automatically turn down for the IoT service to work, and will try to dial again in 2 hours Please check the status of your SIM card" > /var/cell_poweroff.txt
			gsm_poweroff
			echo $(date +%s) > /usr/share/cell_disable.txt
		fi
	elif [ -f "/usr/share/cell_disable.txt" ]; then
		current_time=$(date +%s)
		disable_time=$(cat /usr/share/cell_disable.txt)
		if [  `expr $current_time - $disable_time` -gt 7200  ]; then
			gsm_poweron
			rm /usr/share/cell_disable.txt
		fi
	fi
}

toggle_3g()
{
    GSM_IF_SUF=`ifconfig |grep '3g-' | awk '{print $1}' | awk -F '-' '{print $2}'`
	ifdown $GSM_IF_SUF
    sleep 240
    ifup $GSM_IF_SUF
    logger -t iot_keep_alive "Restart 3G Interface"
}

enable_ue_usage()
{
	killall comgt;
	gcom -d /dev/ttyModemAT -s /etc/gcom/enable-usage.gcom > /var/iot/cell_usage.txt
	if [ `cat /var/iot/cell_usage.txt | grep "Start" -c` -ge "1" ]; then
		if [ `cat /var/iot/cell_usage.txt | grep "successfully" -c` -ge "1"]; then
			ENABLE_USAGE="1"
		fi
	fi

}

station_time_check()
{
	currentype="$(uci -q get gateway.general.server_type)"
	if [ "$currentype" = "station" ]; then
		if [ "$station_check_time" = "1" ]; then 
			syscurrent_time="$(date +"%Y-%m-%d")" 
			stationcurrent_time="$(cat /var/iot/station.log | grep 20 | awk '{print $1}' | grep 20 | sed -n '1p' )" 
			t1="$(date -d "$syscurrent_time" +%s)" 
			t2="$(date -d "$stationcurrent_time" +%s)" 
			if [ $t1 -ne $t2 ]; then
				/usr/bin/reload_iot_service.sh &
				station_check_time=0
			fi 
		fi 
	else
		station_check_time=0
	fi
}


chk_wlan0_client()
{
	local check_link
	local check_link
	local sta_ssid
	local check_scan
	local check_auth
	
	check_link=$(iw dev wlan0-2 link|grep Not -c)
	sta_ssid=$(uci get wireless.sta_0.ssid)
	if [ "$STA_DISENABLE" = "0" ];then
		if [ "$check_link" = 1 ]; then
		check_scan=$(iwinfo radio0 scan | grep "$sta_ssid" -c)
		check_auth=$(dmesg -r |grep "wlan0-2"|grep deauthenticated -c)
		logread_hadle=$(logread |grep handle_probe_req -c)
			if [ "$check_auth" -gt "5" ] || [ "$logread_hadle" -gt 20 ]; then
				logger -t iot_keep_alive "wifi client multiple authentication failures, probably wrong password"
				echo "Wifi client multiple authentication failures, probably wrong password and automatically turn off wifi function" > /usr/share/wifi_handle.txt
				uci set wireless.sta_0.disabled=1 && uci commit wireless
				wifi reload
				dmesg -c > /dev/null 2>&1
				return
			fi
		
			if [ ! -z "$check_scan" ] && [ "$check_scan" -ge "1" ]; then
				wifi
			else
				logger -t iot_keep_alive "wifi_client setting incorrect or AP not in range"
				echo "Detection of wifi not in range and then automatically turn off wifi function, and will auto reconnect once wifi is scaned" > /usr/share/wifi_handle.txt
				last_handle_fail_time=`date +%s` && echo "$last_handle_fail_time" > /var/log/last_handle_fail_time.txt;
				uci set wireless.sta_0.disabled=1 && STA_DISENABLE=1
				uci commit wireless
				wifi reload
				wifi_sta_off_flag=1 && echo 1 >> /usr/share/wifi_sta_off_flag
			fi
		fi
	elif [ "$wifi_sta_off_flag" = "1" ]; then
		check_scan=$(iwinfo radio0 scan | grep "$sta_ssid" -c)
		if [ ! -z "$check_scan" ] && [ "$check_scan" -ge "1" ]; then
			uci set wireless.sta_0.disabled=0 && uci commit wireless
			STA_DISENABLE=0
			wifi reload
			wifi_sta_off_flag=0
			[ -f /usr/share/wifi_handle.txt ] && rm /usr/share/wifi_handle.txt
			[ -f /usr/share/wifi_sta_off_flag ] && rm /usr/share/wifi_sta_off_flag
		fi
	fi
}

while :
do 
	sleep "$iot_interval"
	chk_internet_connection

	# Control receive size < 2M
	receive_size=`du -h -k /var/iot/ -d 0 | awk '{print $1}'`
	if [ $receive_size -gt 1024 ];then
		rm -rf /var/iot/receive/*
		rm -rf /var/iot/channels/*
		rm -f /var/iot/station.log
	fi

	#check WiFi Client settings, handle in case AP-SSID is not present or login incorrect
	[ -f /usr/share/wifi_sta_off_flag ] && wifi_sta_off_flag=1
	STA_DISENABLE=$(uci get wireless.sta_0.disabled)
	if [ "$STA_DISENABLE" = "0" ] || [ "$wifi_sta_off_flag" = "1" ]; then
		chk_wlan0_client
	fi

	if [ "$global_ping" -gt "$ZERO" ];then   # Check if the device has internet connection
		has_internet_flag_time=$(date +%s) #record the time when the network is available
		ROUTE_DF=`ip route | grep default | awk '{print $5}'`
		logger -t iot_keep_alive "Internet Access OK: via $ROUTE_DF"
		has_internet=1
		station_time_check
		if [ "$ROUTE_DF" = "$WAN_IF" ] || [ "$ROUTE_DF" = "$WIFI_IF" ];then  #Check If device has WIFi or WAN Connection.
			logger -t iot_keep_alive "use WAN or WiFi for internet access now"
			gsm_status_check
		elif [ "$ROUTE_DF" = "$GSM_IF" ] && [ "`uci get network.cellular.backup`" = "1" ];then
			chk_eth1_connection
			if [ "$wan_ping" -gt "$ZERO" ];then
				use_wan_as_gateway
			else 
				logger -t iot_keep_alive "XX ping ETH1 $PING_WAN_HOST via eth1 fail"
				chk_wlan0_connection
				if [ "$wifi_ping" -gt "$ZERO" ];then
					[ -f /var/log/last_handle_fail_time.txt ] && rm /var/log/last_handle_fail_time.txt
					use_wifi_as_gateway
				else
					logger -t iot_keep_alive "XX ping WiFi $PING_WIFI_HOST via wlan0-2 fail"
				fi
			fi
		fi
	else
		has_internet=0
		internetdetect=$(uci -q get system.@system[0].internet_detect)
		if [ "$internetdetect" == "checked" ] || [ -z $internetdetect ];then                                                                                                                                                                                                     
            cur_flag_time=$(date +%s)                                                                                                                     
            if [ -z $has_internet_flag_time ] ; then                                                                                                       
                    has_internet_flag_time=$(date +%s)                                                                                                    
            else                                                                                                                                                                                                              
                    time_diff=$((cur_flag_time - has_internet_flag_time))                                                                       
                    if [ $time_diff -gt 900 -a $time_diff -le 3000 ]; then                                                                            
                            reboot   #Execute reboot if the gateway loses Internet connectivity for more than 900 seconds                                 
                    fi                                                                                                                                                                         
            fi                                                                                                                                                                                 
        fi

		if [ "$iot_online" = "0" ] && [ "`uci get gateway.general.server_type`" = "lorawan" ] || [ "`uci get gateway.general.server_type`" = "station" ]; then
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
					[ -f /var/log/last_handle_fail_time.txt ] && rm /var/log/last_handle_fail_time.txt
				else
					logger -t iot_keep_alive "XX ping WiFi $PING_WIFI_HOST via wlan0-2 fail"
					if [ "`uci get network.cellular.auto`" = "1" ];then
						check_3g_connection
						if [ "$gsm_ping" -eq "$ZERO" ]; then
							retry_gsm=`expr $retry_gsm + 1`
							if [ "`expr $retry_gsm % $RETRY_POWEROFF_GSM`" -eq 0 ];then
								gsm_poweroff 
								sleep 5
								gsm_poweron
							elif [ "`expr $retry_gsm % $RETRY_REBOOT_GSM`" -eq 0 ];then
								reboot
							fi
						fi
					fi
					if [ "$gsm_ping" -gt "$ZERO" ]; then
						has_internet=1
						use_gsm_as_gateway
						sleep 10;
						[ "`uci get gateway.general.server_type`" = "lorawan" ] || [ "`uci get gateway.general.server_type`" = "station" ]  && reload_iot_service && logger use_gsm
					else
					#All Interface doesn't have internet connection, reset all. 
						logger -t iot_keep_alive "No internet at any interface"
						update_gateway $WAN_IF
						update_gateway $WIFI_IF
					fi 
				fi
			fi
		else
			logger -t iot_keep_alive "No Internet Connection but IoT Service Online,No Action"
		fi 
	fi

	#opkg update cache check
	gwid=$(uci -q get gateway.general.GWID)
	if [ "$gwid" == "a840411ecc104150" ]; then
		if [ -f /tmp/backup.tar.gz ]; then
			tar xvpfz /tmp/backup.tar.gz -C /
			/usr/bin/reload_iot_service.sh &
		else
			/rom/etc/uci-defaults/rename_network_on_first_boot

		fi
	fi

	#Show LED status
	# echo 1 > /sys/class/leds/dragino2\:red\:system/brightness GPIO28
	# LPS8:GPIO21: RED, GPIO28: Blue GLobal
	# LGxx: GPIO21: N/A. GPIO28: RED Global
	# LIG16: GPIO21(LOW): GREEN; GPIO28: RED; GPIO22(Low), RED
	# Check IoT Connection first. 
	#echo "iot"
	if [ "$is_lps8" = "1" ];then
		if [ "`ls /sys/class/gpio/ | grep -c gpio21`" = "0" ];then
			echo 21 > /sys/class/gpio/export
			echo out > /sys/class/gpio/gpio21/direction
		fi
	fi

	if [ "`uci get gateway.general.server_type`" == "lorawan" ]; then
		if [ -z $(pgrep fwd) ];then
			logger -t iot_keep_alive "IoT Server is not Runing, So Reload IoT Service_flag"
			reload_iot_service
		else
			status_count=$(sqlite3 /var/lgwdb.sqlite "select * from gwdb where key like '/service/lorawan/server/network';")
			if [ -z $status_count ]; then
				status_count=$(sqlite3 /var/lgwdb.sqlite "select * from gwdb;"|grep network |grep online -c)
			else
				status_count=$(sqlite3 /var/lgwdb.sqlite "select * from gwdb where key like '/service/lorawan/server/network';" |grep online -c)
			fi

			if [ "$status_count" -gt 0 ];then
				echo "online" > /var/iot/status
			else
				offline_count=$(sqlite3 /var/lgwdb.sqlite "select * from gwdb;"|grep network |grep offline -c)
				if [ $offline_count -eq 15 ]; then
					if [ -z $flag_time ]; then
						flag_time=$(date +%s)
					else
						cur_flag_time=`date +%s`
						if [ `expr $cur_flag_time - $flag_time` -gt 90 ]; then
							echo "offline" > /var/iot/status
						fi
					fi
				fi
			fi
		fi
		iot_online=`cat /var/iot/status | grep online -c`
	fi

	if [ "`uci get gateway.general.server_type`" == "station" ]; then
		station_status=$(tail /var/iot/station.log | grep -e "HTTP connect failed" -e "failed" -e "Interaction with CUPS failed" -c)
		station_abandon=$(cat /var/iot/station.log | grep -e "abandoning" -c)
		station_threshol=$(cat /var/iot/station.log | grep -e  "threshold 100.0ppm" -c)
		if [ "$station_abandon" -gt "0" ] || [ "$station_threshol" -gt "0" ] ; then
			rm -f /var/iot/station.log
			/usr/bin/reload_iot_service.sh &
			sleep 15;
		fi
		if [ $station_status -gt "0" ]; then
			iot_online=0
			echo "offline" > /var/iot/status
		else
			iot_online=1
			echo "online" > /var/iot/status
		fi
	fi

	if [ "`uci get gateway.general.server_type`" == "mqtt" ] && [ "`uci get mqtt.common.sub_enable`" == "checked" ]; then
        mqtt_subpid=`pgrep mosquitto_sub`                                                                  
        if [ -z $mqtt_subpid ]; then                                    
            /etc/init.d/iot reload                                  
        fi                                                         
    fi   

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
			logger -t iot_keep_alive "Reload IoT Service_flag"
			reload_iot_service
			sleep 30
		fi
	else 
		# IoT Connection Fail, Internet Down
		echo 0 > /sys/class/leds/dragino2\:red\:system/brightness
		[ "$is_lps8" = "1" ] && echo 1 > /sys/class/gpio/gpio21/value
	fi
done