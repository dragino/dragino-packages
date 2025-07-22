#!/bin/sh


model=$(cat /var/iot/model.txt)

##################################################

# Data collection for info boxes

################
# Centre Data - System
model0=$model
firmware0=$(cat /etc/banner | grep Version | awk '{print $3}')
system0=$(cat /etc/os-release | grep _RELEASE | cut -d = -f2)
load0=$(uptime | sed -n 's/average:/&\n/;s/.*\n//p')
ip0=$(uci -q get network.lan.ipaddr)

# Info bar data
system_time=$(date)
uptime_str=$(uptime  | cut -d " " -f4,5 | tr , " ")

SAT1()
{
################
# SAT1 Data - WiFi WAN

STA_DISABLE=$(uci get wireless.sta_0.disabled)
if [ $STA_DISABLE == 1 ]; then
	return
fi

info_title1="WiFi Internet"
ssid1=$(iwinfo wlan0-2 info | grep ESSID |cut -d : -f 2)
ip1=$(ifconfig wlan0-2|grep "inet addr"|cut -d ":" -f 2|cut -d " " -f 1)
txb1=$(ifconfig wlan0-2 |grep "TX bytes"|cut -d " " -f 18-20)
rxb1=$(ifconfig wlan0-2 |grep "RX bytes"|cut -d " " -f 13-15)
signal1=$(iwinfo|grep -A 5 wlan0-2 | grep Signal: | cut -d " " -f 11-13)
noise1=$(iwinfo|grep -A 5 wlan0-2 | grep Signal: | cut -d " " -f 15-17)
rate1=$(iwinfo|grep -A 5 wlan0-2 | grep Rate: | cut -d " " -f 11-15)

}

SAT2()
{
################
# SAT2 Data - Eth WAN

info_title2="Cable Internet"
ip2a=$(ifconfig eth1|grep "inet addr"|cut -d ":" -f 2|cut -d " " -f 1)
ip2b=$(ifconfig eth1:9|grep "inet addr"|cut -d ":" -f 2|cut -d " " -f 1)
ip2="$ip2a  $ip2b" 
txb2=$(ifconfig eth1 |grep "TX bytes"|cut -d " " -f 18-20)
rxb2=$(ifconfig eth1 |grep "RX bytes"|cut -d " " -f 13-15)
}

SAT3()
{
################
# SAT3 Data - IoT Service

# Get server type
server_type=$(uci get gateway.general.server_type)
# Initialise
process3=" "
status3="0"
server3=" "
if [ $server_type == "lorawan" ]; then
	info_title3="LoRaWAN Service"
	server3=$(uci get gateway.server1.server_address)  
	process3="LoRaWAN process fwd <b>Running</b>"
	status3=$(cat /var/iot/status)
	if [ -z "$(pgrep fwd)" ]; then 
		process3="LoRaWAN process fwd <b>Not Running</b>"
	fi
elif [ $server_type == "station" ]; then
	info_title3="LoRaWAN Basic Station"
	server_provider=`uci get gateway.general.station_server_provider`
	
	if [ "$server_provider" == "AWS" ]; then
		server3=AWS,`cat /etc/station/cups.uri`
	elif [ "$server_provider" == "TTN" ]; then
		server3=TTN,`cat /etc/station/cups.uri`
	elif [ "$server_provider" == "CS" ]; then
		server3=Chirpstack,`cat /etc/station/tc.uri`
	elif [ "$server_provider" == "SN" ]; then
		server3=Senet,`cat /etc/station/tc.uri`
	elif [ "$server_provider" == "TP" ]; then
		server3=ThingPart,`cat /etc/station/tc.uri`
	elif [ "$server_provider" == "LR" ]; then
		server3=LORIOT,`cat /etc/station/tc.uri`
	elif [ "$server_provider" == "CW" ]; then
		server3="Chirp Wireless,$(cat /etc/station/tc.uri)"
	fi
	status3=$(cat /var/tmp/station_status.log)
	process3="Station is <b>Running</b>"
	station_pid=$(pgrep station)
	if [ -z "$station_pid" ]; then
		process3="Station is <b>Not Running</b>"
	fi

elif [ $server_type == "loriot" ]; then
	info_title3="LORIOT Service"
	server3=$(uci -q get loriot.loriot.url)
	version3=$(uci -q get loriot.loriot.version)
	loriotps=$(ps | grep -c loriot_dragino)
	if [ $loriotps == "2" ];then
		process3="LORIOT process <b>Running</b>"
	else
		process3="LORIOT process <b>Not Running</b>"
	fi

elif [ $server_type == "mqtt" ]; then
	info_title3="MQTT Service"
	server3=$(uci -q get mqtt.common.server_type)
	pubstatus=$(ps | grep -c mqtt_process)
	substatus=$(ps | grep -c mosquitto_sub)
	if [ $pubstatus -ge "2" ] || [ $substatus == "2" ]; then
		process3="MQTT process(es) <b>Running</b>"
		status3=$(cat /var/iot/status)
	else
		process3="MQTT process <b>Not Running</b>"
	fi

elif [ $server_type == "tcpudp" ]; then
	info_title3="TCP/UDP Service"
	server3=$(uci -q get tcp_client.general.server_address)
	tcpstatus=$(ps | grep -c tcp_process)
	if [ $tcpstatus == "2" ]; then
		process3="TCP/UDP process <b>Running</b>"
		status3=$(cat /var/iot/status)
	else
		process3="TCP/UDP process <b>Not Running</b>"
	fi
	
elif [ $server_type == "http" ]; then
	info_title3="HTTP Service"
	server3=$(uci -q get http_iot.general.server_type)
	httpstatus=$(ps | grep -c http_process)
	if [ $httpstatus == "2" ];then
		process3="HTTP process <b>Running</b>"
		status3=$(cat /var/iot/status)
	else
		process3="HTTP process <b>Not Running</b>"
	fi
	
elif [ $server_type == "customized" ]; then
	info_title3="Custom Service"
	script_name=$(uci get customized_script.general.script_name)
	customstatus=$(ps | grep -c $script_name)
	if [ $customstatus -ge 2 ];then
		process3=" $script_name <b>Running</>"
		status3=$(cat /var/iot/status)
	else
		process3=" $script_name <b>Not Running</>"
	fi

elif [ $server_type == "relay" ]; then
	info_title3="LoRaWAN Relay Service"
	server3=" "
	relaystatus=$(ps | grep -c pkt_fwd)
	if [ $relaystatus == "2" ];then
		process3="Process pkt_fwd <b>Running</b>"
		status3=$(cat /var/iot/status)
	else
		process3=" Process pkt_fwd <b>Not Running</b>"
	fi
fi
}

SAT5()
{
################
# SAT5 Data - Cellular WAN

    CELLULAR_DISABLE=$(uci -q get network.cellular.auto) # Is interface enabled
    if [ $CELLULAR_DISABLE == 0 ]; then
		return
    fi

    info_title5="Cellular Internet"
    ip5=$(ifconfig wwan0|grep "inet addr"|cut -d ":" -f 2|cut -d " " -f 1)
    txb5=$(ifconfig wwan0 |grep "TX bytes"|cut -d " " -f 18-20)
    rxb5=$(ifconfig wwan0 |grep "RX bytes"|cut -d " " -f 13-15)

  # Get cell status and save to file
	cp /tmp/celltmp.txt /tmp/cell1.txt 
	# killall  -q comgt ;
  
	# if [ "$(cat /sys/kernel/debug/usb/devices | grep "Vendor=1e0e ProdID=9011" -c)" == "1" ]; then
	# 	(comgt -d /dev/ttyUSB3 > /tmp/celltmp.txt; ) &
	# 	if [ -f /tmp/celltmp.txt ]; then
	# 		cops_format=$(cat /tmp/celltmp.txt  | awk NR==3 |  cut -c 30-34)
	# 		if [ "$cops_format" -gt "0" ]; then
	# 			comgt -d /dev/ttyUSB1 -s /etc/gcom/setcopsfromat.gcom 
	# 		fi
	# 	fi
	# else
	# 	if [ "$model" = "LPS8-N" ]; then
	# 		( comgt -d /dev/ttyUSB3 > /tmp/celltmp.txt )  &
	# 	else	
	# 		( comgt -d /dev/ttyModemAT > /tmp/celltmp.txt ) &
	# 	fi
	# fi
    ( comgt > /tmp/celltmp.txt )  &
  # Extract data for Info box
    sim5=$(cat /tmp/cell1.txt |grep SIM)
    sig=$(cat /tmp/cell1.txt | grep Signal)
    net5=$(cat /tmp/cell1.txt | grep network: | cut -d : -f 2)
  

#  sim5=$(comgt PIN|awk NR==2)
#  sig=$(comgt sig|awk NR==2)
#  net5=$(comgt reg|awk -F':' 'NR==3 {print $2}')

  # Get signal in dBm
  signal=$(echo $sig | cut -d : -f2 | cut -d , -f1)
  echo $signal
  if [ $signal -ge "2" ] && [ $signal -le "30" ]; then
  	dbm=$(expr $signal + $signal - "113")
  	sig5="$sig  $dbm dBm" 
	else
		sig5=" $sig"
	fi
	#Fast Cell Internet check	
	# host1="1.1.1.1"                                     
	# host2="www.dragino.com"
	# fping -q -I wwan0 $host1 
	# if [ $? -eq "0" ]; then
  	# internet5="OK"
	# else
	# 	fping -q -I wwan0 $host2
	# 	if [ $? -eq "0" ]; then
  	# 	internet5="OK"
	# 	else
	# 		internet5="<font size="2" color="red">Fail</font"
	# 	fi
	# fi
	if [ $(cat /var/iot/internet) = 1 ]; then
		internet5="OK"
	else
		internet5="<font size="2" color="red">Fail</font"
	fi
}

SAT10()
{
################
# SAT10 Data - LoRa Radios
# Check for LORIOT mode
if [ $server_type == "loriot" ]; then
	info_title10="LORIOT Mode"
	server10="$server3"
elif [ $server_type == "station" ];then
	info_title10="LoRaWAN Basic Station"
	server10="Station: 3.0.2(mips-openwrt/dragino) 2024-03-28 10:45:52"
else
	info_title10="LoRa Radio"
fi

if [ $model == "LG01" ];then
	rxfreq10=$(uci get gateway.radio1.RFFREQ)
	txfreq10=$(uci get gateway.radio1.RFFREQ)
	rxbw10=$(uci get gateway.radio1.RFBW)
	txbw10=$(uci get gateway.radio1.RFBW)
	rxcr10=$(uci get gateway.radio1.RFCR)
	txcr10=$(uci get gateway.radio1.TFCR)
	rxsf10=$(uci get gateway.radio1.RFSF)
	txsf10=$(uci get gateway.radio1.TFSF)
elif [ $model == "LG02" ];then
	rxfreq10=$(uci get gateway.radio1.RXFREQ)
	txfreq10=$(uci get gateway.radio2.TXFREQ)
	rxbw10=$(uci get gateway.radio1.RXBW)
	txbw10=$(uci get gateway.radio2.TXBW)
	rxcr10=$(uci get gateway.radio1.RXCR)
	txcr10=$(uci get gateway.radio2.TXCR)
	rxsf10=$(uci get gateway.radio1.RXSF)
	txsf10=$(uci get gateway.radio2.TXSF)
else
	gwcfg=$(uci get gateway.general.gwcfg)
	subband=$(uci get gateway.general.subband)
	
	band10=$(grep -e \"$gwcfg\" /www/cgi-bin/inc/band.inc | cut -d ">" -f 2 | cut -d "<" -f 1)
	
	if [ $gwcfg == "AU" ]; then
		subband10=$(grep -e \"$subband\" /www/cgi-bin/inc/subband-au.inc | cut -d ">" -f 2 | cut -d " " -f 1,2,3)
	elif [ $gwcfg == "US" ]; then
		subband10=$(grep -e \"$subband\" /www/cgi-bin/inc/subband-us.inc | cut -d ">" -f 2 | cut -d " " -f 1,2,3)
	elif [ $gwcfg == "CUS" ]; then
		band10="Custom"
		subband10=" " 
	else
		subband10=" "
	fi
fi
}

SAT11()
{
################
# SAT11 Data - WiFi Access Point
info_title11="WiFi Access point"

ap_disable=$(uci -q get wireless.ap_0.disabled)
if [ $ap_disable == "1" ]; then
	sat11="/static/img/SAT-Wifi-off.png"
    return
fi

ssid11=$(iwinfo wlan0 info | grep ESSID |cut -d : -f 2)
chan11=$(iwinfo wlan0 info | grep Channel |cut -d : -f 3)
mode11=$(iwinfo wlan0 info | grep "HW Mode" |cut -d : -f 3)
txb11=$(ifconfig wlan0 |grep "TX bytes"|cut -d " " -f 18-20)
rxb11=$(ifconfig wlan0 |grep "RX bytes"|cut -d " " -f 13-15)
}

SAT1
SAT2
SAT3
SAT5
SAT10
SAT11

#######################################

# Create the temporay txt file

cat > /tmp/popup-data.txt << EOF


<div class="info" id="info-0">
	<table>
		<tr>	  <th colspan="2">System Info</th></tr>
		<tr>	  <td>Model:</td><td>$model0 </td>	</tr>
		<tr>	  <td>Firmware:</td><td>$firmware0 </td>	</tr>
		<tr>	  <td>System:</td><td>$system0 </td>	</tr>
		<tr>	  <td>LAN IP:</td><td>$ip0 </td>	</tr>
		<tr>	  <td>Load Avg:</td><td>$load0 </td>	</tr>
		<tr>	  <td>Date:</td><td>$system_time </td>	</tr>
		<tr>	  <td>Uptime:</td><td>$uptime_str </td>	</tr>
		</table>
</div>	

<div class="info" id="info-1">
	<table>
	<tr>	  <th colspan="2">$info_title1 </th>	</tr>
	<tr>	  <td>SSID:</td><td>$ssid1 </td>	</tr>
	<tr>	  <td>IP Addr:</td><td>$ip1 </td>	</tr>
	<tr>	  <td>TX Bytes:</td><td>$txb1 </td>	</tr>
	<tr>	  <td>RX Bytes:</td><td>$rxb1 </td>	</tr>
	<tr>	  <td>Signal:</td><td>$signal1 </td>	</tr>
	<tr>	  <td>Noise:</td><td>$noise1 </td>	</tr>
	<tr>	  <td>Bit Rate:</td><td>$rate1 </td>	</tr>
	</table>
</div>	

<div class="info" id="info-2">
	<table>
	<tr>	  <th colspan="2">$info_title2 </th>	</tr>
	<tr>	  <td>IP Addr:</td><td>$ip2 </td>	</tr>
	<tr>	  <td>TX Bytes:</td><td>$txb2 </td>	</tr>
	<tr>	  <td>RX Bytes:</td><td>$rxb2 </td>	</tr>
	</table>
</div>	

<div class="info" id="info-3">
	<table>
	<tr>	  <th colspan="2">$info_title3 </th>	</tr>
	<tr>	  <td>Process:</td><td>$process3 </td>	</tr>
	<tr>	  <td>Status:</td><td>$status3 </td>	</tr>
	<tr>	  <td>Server:</td><td>$server3 </td>	</tr>
	</table>
</div>	

<div class="info" id="info-3a">
	<table>
	<tr>	  <th colspan="2">$info_title3 </th>	</tr>
	<tr>	  <td>Process:</td><td>$process3 </td>	</tr>
	<tr>	  <td>Ver:</td><td>$version3 </td>	</tr>
	<tr>	  <td>Server:</td><td>$server3 </td>	</tr>
	</table>
</div>	

<div class="info" id="info-5">
	<table>
	<tr>	  <th colspan="2">$info_title5 </th>	</tr>
	<tr>	  <td>IP Addr:</td><td>$ip5 </td>	</tr>
	<tr>	  <td>TX Bytes:</td><td>$txb5 </td>	</tr>
	<tr>	  <td>RX Bytes:</td><td>$rxb5 </td>	</tr>
	<tr>	  <td>SIM:</td><td>$sim5 </td>	</tr>
	<tr>	  <td>Network:</td><td>$net5 </td>	</tr>
	<tr>	  <td>Signal:</td><td>$sig5 </td>	</tr>
	<tr>	  <td>Internet:</td><td><b>$internet5</b></td>	</tr>
	</table>
</div>	

<div class="info" id="info-10a">
	<table>
		<tr>	  <th colspan="2">$info_title10 </th>	</tr>
		<tr>	  <td>Rx Freq:</td><td>$rxfreq10 </td>	</tr>
		<tr>	  <td>Tx Freq:</td><td>$txfreq10 </td>	</tr>
		<tr>	  <td>Rx BW / CR / SF:</td><td>$rxbw10 / $rxcr10 / $rxsf10 </td>	</tr>
		<tr>	  <td>Tx BW / CR / SF:</td><td>$txbw10 / $txcr10 / $txsf10 </td>	</tr>
	</table>
	</div>	

<div class="info" id="info-10b">
	<table>
		<tr>	  <th colspan="2">$info_title10 </th>	</tr>
		<tr>	  <td>Freq Band:</td><td>$band10</td>	</tr>
		<tr>	  <td>Sub Band:</td><td>$subband10</td>	</tr>
	</table>
	</div>	

<div class="info" id="info-10c">
	<table>
		<tr>	  <th colspan="2">$info_title10 </th>	</tr>
		<tr>	  <td>Server:</td><td>$server10</td>	</tr>
	</table>
</div>	

<div class="info" id="info-10d">
	<table>
		<tr>	  <th colspan="2">$info_title10 </th>	</tr>
		<tr>	  <td>Version:</td><td>$server10</td>	</tr>
	</table>
</div>

<div class="info" id="info-11">
	<table>
	<tr>	  <th colspan="2">$info_title11 </th>	</tr>
	<tr>	  <td>SSID:</td><td>$ssid11 </td>	</tr>
	<tr>	  <td>Channel:</td><td>$chan11 </td>	</tr>
	<tr>	  <td>Mode:</td><td>$mode11 </td>	</tr>
	<tr>	  <td>TX Bytes:</td><td>$txb11 </td>	</tr>
	<tr>	  <td>RX Bytes:</td><td>$rxb11 </td>	</tr>
	</table>
</div>	

|||$system_time|||$uptime_str

EOF
#####################