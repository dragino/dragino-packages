#!/bin/sh

mouseover0=''
mouseover1=''
mouseover2=''
mouseover3=''
mouseover4=''
mouseover5=''
mouseover6=''
mouseover7=''
mouseover8=''
mouseover9=''
mouseover10=''
mouseover11=''
mouseover12=''

#Initialise (clear) all SAT images and links

satlink1=""
satlink2=""
satlink3=""
satlink4=""
satlink5=""
satlink6=""
satlink7=""
satlink8=""
satlink9=""
satlink10=""
satlink11=""
satlink12=""

sat1="/static/img/SAT-Space-Blank.png"
sat2="/static/img/SAT-Space-Blank.png"
sat3="/static/img/SAT-Space-Blank.png"
sat4="/static/img/SAT-Space-Blank.png"
sat5="/static/img/SAT-Space-Blank.png"
sat6="/static/img/SAT-Space-Blank.png"
sat7="/static/img/SAT-Space-Blank.png"
sat8="/static/img/SAT-Space-Blank.png"
sat9="/static/img/SAT-Space-Blank.png"
sat10="/static/img/SAT-Space-Blank.png"
sat11="/static/img/SAT-Space-Blank.png"
sat12="/static/img/SAT-Space-Blank.png"


#Hide all SAT images and links - Visibility should be set to "visible" only if there is a (non-blank) image to display.

satvis1="hidden"
satvis2="hidden"
satvis3="hidden"
satvis4="hidden"
satvis5="hidden"
satvis6="hidden"
satvis7="hidden"
satvis8="hidden"
satvis9="hidden"
satvis10="hidden"
satvis11="hidden"
satvis12="hidden"


# Generate the txt files every 10 seconds

# Get model type
model=$(cat /tmp/iot/model.txt)

# Get base network config data
cable=$(ifconfig | grep -c eth1)
wifi=$(ifconfig  | grep -c wlan0-2)

cell_en=$(uci -q get network.cellular.auto) # Is interface enabled
cell_if=$(ifconfig  | grep -c wwan0) # Is interface present

# Get server type
server_type=$(uci get gateway.general.server_type)

# Get default route
route=$(ip route|grep default | cut -d " " -f 5|awk NR==1)

# Check Internet connectivity
host1="1.1.1.1"
host2="www.dragino.com"

internet=`cat /var/iot/internet`


##################################
# SAT Display Data

################
# Setup Centre - System
# Nothing to do

################
# Setup SAT1 - WiFi WAN
SAT1()
{
satvis1="visible"
img1="/static/img/SAT-Int-Wifi"
satlink1="/cgi-bin/system-wifi.has"
mouseover1='info-1'

wifi_wan_disable=$(uci -q get wireless.sta_0.disabled)
if [[ $wifi_wan_disable == "1" ]]; then   # No WiFiWAN icon reqd if not enabled
  sat1="/static/img/SAT-Space-Blank.png"
  mouseover1=""
  satlink1=""
  satvis1="hidden"
  return
fi

wifi_wan=$(ip route|grep -c wlan0-2)
# Set up the SAT icon
if [ $internet == "1" ] && [[ $route == "wlan0-2" ]]; then
  sat1=$img1"-tick.png"
elif [[ $wifi_wan -gt "0" ]]; then
  sat1=$img1"-tick-amber.png"
else
  sat1=$img1"-cross.png"
fi
}

################
# Setup SAT2 - Eth WAN
SAT2()
{
satvis2="visible"
img2="/static/img/SAT-Int-Cable"
satlink2="/cgi-bin/system-network.has"
mouseover2='info-2'

eth_wan=$(ubus call network.device status '{"name":"eth1"}' | grep '"carrier"'| awk '{print $2}' | sed 's/,//g')

# Set up the SAT icon
if [ $internet == "1" ] && [ $route == "eth1" ]; then
  sat2=$img2"-tick.png"
elif [ $eth_wan == "true" ]; then  
  sat2=$img2"-tick-amber.png"
else
  sat2=$img2"-cross.png"
fi
}

SAT3()
{
################
# Setup SAT3 - IoT Service

#	For all the conditions below:
	satvis3="visible"	
	mouseover3='info-3'

if [ $server_type == "disabled" ]; then
	satlink3="/cgi-bin/lora-lora.has"
	sat3="/static/img/SAT-Disabled.png"
		
elif [ $server_type == "loriot" ]; then
	satlink3="/cgi-bin/loriot.has"
	sat3="/static/img/SAT-Loriot.png"
	mouseover3='info-3a'
elif [ $server_type == "station" ]; then
	satlink3="/cgi-bin/lorawan-basicstation.has"
	station_status=$(cat /var/tmp/station_status.log)
	if [ $station_status = "ONLINE" ]; then
		sat3="/static/img/SAT-LoRaWAN-tick.png"
	else
		sat3="/static/img/SAT-LoRaWAN-cross.png"
	fi

elif [ $server_type == "lorawan" ]; then
	satlink3="/cgi-bin/lorawan.has"
		status=$(cat /var/iot/status)
	if [ $status == "online" ];then
		sat3="/static/img/SAT-LoRaWAN-tick.png"
	else
		sat3="/static/img/SAT-LoRaWAN-cross.png"
		
	fi

elif [ $server_type == "mqtt" ]; then
	satlink3="/cgi-bin/mqtt.has"
	pubstatus=$(ps | grep -c mqtt_process)
	substatus=$(ps | grep -c mosquitto_sub)
	if [ $pubstatus -ge "2" ] || [ $substatus == "2" ]; then
		sat3="/static/img/SAT-MQTT-tick.png"
		mqttstatus="1"
	else
		sat3="/static/img/SAT-MQTT-cross.png"
	fi
	
elif [ $server_type == "tcpudp" ]; then
	satlink3="/cgi-bin/tcp-client.has"
	tcpstatus=$(ps | grep -c tcp_process)
	if [ $tcpstatus == "2" ];then
		sat3="/static/img/SAT-TCP-tick.png"
	else
		sat3="/static/img/SAT-TCP-cross.png"
	fi
	
elif [ $server_type == "http" ]; then
	satlink3="/cgi-bin/http.has"
	httpstatus=$(ps | grep -c http_process)
	if [ $httpstatus == "2" ];then
		sat3="/static/img/SAT-HTTP-tick.png"
	else
		sat3="/static/img/SAT-HTTP-cross.png"
	fi
	
elif [ $server_type == "customized" ]; then
	satlink3="/cgi-bin/custom.has"
	script_name=$(uci get customized_script.general.script_name)
	customstatus=$(ps | grep -c $script_name)
	if [ $customstatus -ge 2 ];then
		sat3="/static/img/SAT-Custom-tick.png"
	else
		sat3="/static/img/SAT-Custom-cross.png"
	fi
	
elif [ $server_type == "relay" ]; then
	satlink3="/cgi-bin/lora-lora.has"
	relaystatus=$(ps | grep -c pkt_fwd)
	if [ $relaystatus == "2" ];then
		sat3="/static/img/SAT-Relay-tick.png"
	else
		sat3="/static/img/SAT-Relay-cross.png"
	fi
elif [ $server_type == "abpdecode" ]; then
	satlink3="/cgi-bin/lora-abp.has"
	sat3="/static/img/SAT-ABP-tick.png"
else
  sat3=""
  satlink3=""
  mouseover3=""
fi
}

SAT5()
{
################
# Setup SAT5 - Cellular WAN

satlink5="/cgi-bin/system-cellular.has"

if [ $cell_en == "1" ]; then
  satvis5="visible"
  mouseover5='info-5'
else
  sat5="/static/img/SAT-Space-Blank.png" # No Cell icon reqd if cell not enabled
  mouseover5=''
  satlink5="#"
  satvis5="hidden"
  return
fi

internet_cell=`cat /var/iot/internet`
# Set up the SAT icon
if [ $route == "wwan0" ] && [ $internet_cell == "1" ]; then
  sat5="/static/img/SAT-Int-Cell-tick.png"
elif [ $route != "wwan0" ] && [ $internet_cell == "1" ] && [ $cell_if == "1" ]; then  
	sat5="/static/img/SAT-Int-Cell-tick-amber.png"
elif [ $route != "wwan0" ] && [ $internet_cell == "0" ]; then  
  sat5="/static/img/SAT-Int-Cell-cross-amber.png"
elif [ $cell_if == "0" ]; then
	sat5="/static/img/SAT-Int-Cell-cross.png"
else
	sat5="/static/img/SAT-Int-Cell-cross.png"
fi

}

SAT10()
{
################
# Setup SAT10 - LoRa Radios

# TBD  Other status indicator for LoRa radio operation

if [ $server_type == "loriot" ]; then
	pscount=$(ps | grep -c loriot_dragino) # Check is process is running
	satlink10="/cgi-bin/loriot.has"
elif [ $server_type == "station" ];then
	pscount=$(ps | grep station | grep -c -v grep) # Check is process is running
	satlink10="/cgi-bin/lorawan-basicstation.has"
else
	#new_fwd=$(ps | grep -c /usr/bin/fwd) 	# Check new_fwd or pkt_fwd
	#if [ "$new_fwd" == "2" ] ;then
	if [ $model == "LG01" ] || [ $model == "LG02" ]; then
		pscount=$(ps | grep -c pkt_fwd) # Check is process is running
	else
		fwd_status=`pgrep fwd`
		if [ -n "$fwd_status" ];then
			pscount="2"
		else
			pscount="1"
		fi
	fi
	satlink10="/cgi-bin/lora-lora.has"
fi

if [[ "$pscount" == "2" ]];then
	sat10="/static/img/SAT-LoRa-tick.png"
else
	sat10="/static/img/SAT-LoRa-cross.png"
fi
 
satvis10="visible"

if [ $server_type == "loriot" ]; then
 	mouseover10='info-10c'
elif [ $server_type == "station" ]; then
	mouseover10='info-10d'
elif [ $model == "LG01" ] || [ $model == "LG02" ]; then
 	mouseover10='info-10a'
else
 	mouseover10='info-10b'
fi
}

SAT11()
{
################
# Setup SAT11 - WiFi Access Point

mouseover11='info-11'
satlink11="/cgi-bin/system-wifi.has"
satvis11="visible"

ap_disable=$(uci -q get wireless.ap_0.disabled)
if [ $ap_disable == "1" ]; then
	sat11="/static/img/SAT-Wifi-off.png"
    return
fi

# Check if WiFi channel is displayed to indicate valid operation of AP, with and without WiFi WAN
ap_carrier=$(ubus call network.device status '{"name":"wlan0"}' | grep '"carrier"'| awk '{print $2}' | sed 's/,//g')

if [ $ap_carrier == "true" ];then
 	sat11="/static/img/SAT-Wifi-tick.png"
else
 	sat11="/static/img/SAT-Wifi-cross.png"
fi
}

#########################

# Create the temporary txt file

SAT1
SAT2
SAT3
SAT5
SAT10
SAT11

cat > /tmp/sat-data.txt << EOF

" ",
$sat1,$mouseover1,$satlink1,$satvis1 ,
$sat2,$mouseover2,$satlink2,$satvis2 ,
$sat3,$mouseover3,$satlink3,$satvis3 ,
$sat4,$mouseover4,$satlink4,$satvis4 ,
$sat5,$mouseover5,$satlink5,$satvis5 ,
$sat6,$mouseover6,$satlink6,$satvis6 ,
$sat7,$mouseover7,$satlink7,$satvis7 ,
$sat8,$mouseover8,$satlink8,$satvis8 ,
$sat9,$mouseover9,$satlink9,$satvis9 ,
$sat10,$mouseover10,$satlink10,$satvis10 ,
$sat11,$mouseover11,$satlink11,$satvis11 ,
$sat12,$mouseover12,$satlink12,$satvis12 

EOF