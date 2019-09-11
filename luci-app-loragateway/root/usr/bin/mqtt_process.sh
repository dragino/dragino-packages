#!/bin/sh

logger "MQTT_Process"

#Check If we have set debug
DEBUG=`uci get gateway.general.DEB` 
DEBUG=`echo $DEBUG| awk '{print int($0)}'`

UPDATE_INTERVAL=5

old=`date +%s`

CERTPATH="/etc/iot/cert/"

server_type=`uci get mqtt.general.server_type`
QoS=`uci get mqtt.general.QoS`

# Check if the MQTT Server is a fix server
if [ `uci get mqtt.$server_type.fix_server` == "1" ] ;then
	server=`uci get mqtt.$server_type.server`
else
	server=`uci get mqtt.general.server`
fi

# Get shared MQTT Parameters
user=`uci -q get mqtt.general.username`
pass=`uci -q get mqtt.general.password`
clientID=`uci -q get mqtt.general.client_id` 


# Get server specific MQTT parameters
port=`uci get mqtt.$server_type.port`
pub_format=`uci get mqtt.$server_type.topic_format`
data_format=`uci get mqtt.$server_type.data_format`
cafile=`uci -q get mqtt.$server_type.cafile`

# If cafile is not required then set the fields to null
if [[ -z "$cafile" ]];then
	C=" "
	cafile=""
else
	C="--cafile "
fi

# Set Certificate and Key file parameters
if [[ -e "$CERTPATH"certfile"" ]];then
	cert=$CERTPATH"certfile"
else
	cert=""
fi
if [[ -e "$CERTPATH"keyfile"" ]];then
	key=$CERTPATH"keyfile"
else
	key=""
fi

[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Server:Port" $server:$port
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Topic Format: " $pub_format
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Data Format: " $data_format

#Run Forever
while [ 1 ]
do
	now=`date +%s`
	if [ `expr $now - $old` -gt $UPDATE_INTERVAL ];then
		old=`date +%s`
		CID=`ls /var/iot/channels/`
		[ $DEBUG -ge 2 ] && logger "[IoT.MQTT]: Check for sensor update"
		if [ -n "$CID" ];then
			[ $DEBUG -ge 2 ] && [ -n "$CID" ] && logger "[IoT.MQTT]: Found Data at Local Channels:" $CID
			for channel in $CID; do
				HAS_CID=`uci show mqtt | grep 'local_id=' | grep $channel | awk -F '[][]' '{print $2}'`
				if [ -n "$HAS_CID" ];then
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Find Match Entry for $channel" 

					# Get values
					remote_id=`uci get mqtt.@channels[$HAS_CID].remote_id`
					channel_API=`uci -q get mqtt.@channels[$HAS_CID].write_api_key`
					data=`cat /var/iot/channels/$channel`

					# Generate Topic and Data strings
					# Initialise strings
					topic=$pub_format
					mqtt_data=$data_format
					# Replace topic macros
					topic=`echo ${topic/CHANNEL/$remote_id}`  
					topic=`echo ${topic/CLIENTID/$clientID}`				
					topic=`echo ${topic/WRITE_API/$channel_API}`
					topic=`echo ${topic/USERNAME/$user}`
					# Replace data macros
					mqtt_data=`echo ${mqtt_data/CHANNEL/$remote_id}`
					mqtt_data=`echo ${mqtt_data/DATA/$data}`  

					# Initialise debug flag
					D=" "

					# -----------------------------------------
					# Debug output 
					if [ $DEBUG -ge 1 ]; then
						logger "[IoT.MQTT]:  "
						logger "[IoT.MQTT]:-----"
						logger "[IoT.MQTT]:Parameters"
						logger "[IoT.MQTT]:server[-h]: "$server
						logger "[IoT.MQTT]:port[-p]: "$port
						logger "[IoT.MQTT]:user[-u]: "$user
						logger "[IoT.MQTT]:pass[-P]: "$P$pass
						logger "[IoT.MQTT]:QoS[-q]: "$QoS
						logger "[IoT.MQTT]:cafile[--cafile]: "$cafile
						logger "[IoT.MQTT]:cert[--cert]: "$cert
						logger "[IoT.MQTT]:key[--key]: "$key
						logger "[IoT.MQTT]:clientID[-u]: "$clientID
						logger "[IoT.MQTT]:remote_id: "$remote_id
						logger "[IoT.MQTT]:topic[-t]: "$topic
						logger "[IoT.MQTT]:mqtt_data[-m]: "$mqtt_data
						logger "[IoT.MQTT]:------"
					fi 

					# ------------------------------------------
					# Debug console output for manual testing

					if [ $DEBUG -ge 10 ]; then
						# Set debug flag
						D="-d"
						# Echo parameters to console
						echo " "
						echo "-----"
						echo "Parameters"
						echo "server: "$server
						echo "port: "$port
						echo "user: "$user
						echo "pass: "$P$pass
						echo "QoS: "$QoS
						echo "cert: "$cert
						echo "key: "$key
						echo "cafile: "$cafile
						echo "clientID: "$clientID
						echo "remoteID: "$remote_id
						echo "topic: "$topic
						echo "mqtt_data: "$mqtt_data
						echo "------"
					fi
					# ------------------------------------------

					# Send MQTT Command
					if [ ! -z "$pass" ]; then  # Check for password 
						mosquitto_pub $D -h $server -p $port -q $QoS -i $clientID -t $topic -m "$mqtt_data" $C $cafile -u $user -P "$pass" 
					elif [ ! -z "$cert" ]; then # Check for cert
						mosquitto_pub $D -h $server -p $port -q $QoS -i $clientID -t $topic -m "$mqtt_data" $C $cafile --cert $cert --key $key
					else
						mosquitto_pub $D -h $server -p $port -q $QoS -i $clientID -t $topic -m "$mqtt_data" $C $cafile -u $user 
					fi

					# Delete the Channel info
					rm /var/iot/channels/$channel
				else
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Did Not Find Match Entry for $channel" 
					rm /var/iot/channels/$channel
				fi
			done
		fi
	fi
done

