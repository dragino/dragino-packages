#A script to update status to globacom system. temperory file. need to develop more in future. 
## Copyright (c) 2016 Dragino Tech <support@dragino.com>

LOG_SERVER=`uci get system.log.log_server`
ETH0=`ifconfig | grep -m 1 'eth0' | awk '{gsub(/:/,"")}{print tolower($5)}'`

if [ -z "$LOG_SERVER" ];then
	logger [GLOBACOM ROUTINE NOTICE]: LOG_SERVER is not defined
	exit 0 
fi

logger [GLOBACOM ROUTINE NOTICE]: Update Status to $LOG_SERVER


#Get Signal Quality
GlO_ROUTINE_PID=`ps | grep "glo_routine" | grep -v grep | awk '{print $1}'`
if [ ! -z "$GlO_ROUTINE_PID" ];then
	if [ `cat /sys/devices/platform/leds-gpio/leds/dragino2:red:wlan/brightness` -eq 1 ];then
		signal_quality='poor'
	elif [ `cat /sys/devices/platform/leds-gpio/leds/dragino2:red:wan/brightness` -eq 1 ];then
		signal_quality='normal'
	elif [ `cat /sys/devices/platform/leds-gpio/leds/dragino2:red:lan/brightness` -eq 1 ];then
		signal_quality='good'
	else
		signal_quality='unknown'
	fi	
fi

#Get Internet Access method:
wanport=`uci get secn.wan.wanport`
if [ "$wanport" = "Ethernet" ];then
	internet='wan'
elif [ "$wanport" = "USB-Modem" ];then
	internet='cellular'
fi


#Get and Update Asterisk status
lua /usr/bin/get_voip_status.lua
sleep 2
servers=`uci show voip | grep 'server\[[0-9]\]=server' | awk -F '[][]' '{print $2}'`
for server in `ls /var/voip/server/`;do
	host=`cat /var/voip/server/$server/host`
	for uci_server in $servers; do
		if [ "$host" = "`uci get voip.@server[$uci_server].host`" ];then
			if [ "`uci get voip.@server[$uci_server].protocol`" = "suissephone" ];then
				reg_state=`cat /var/voip/server/$server/state`
				if [ "$reg_state" = "Registered" ];then
					voip_register="1"
				else
					voip_register="0"
				fi
			elif [ "`uci get voip.@server[$uci_server].protocol`" = "globacom" ];then
				reg_state=`cat /var/voip/server/$server/state`
				if [ "$reg_state" = "Registered" ];then
					control_register="1"
				else
					control_register="0"
				fi			
			fi
		fi
	done 
done

curl -d "inet_type=${internet}&signal_strenght=${signal_quality}&sip_voip_registered=${voip_register}&sip_control_registered=${control_register}" "http://${LOG_SERVER}/api/${ETH0}/update"
