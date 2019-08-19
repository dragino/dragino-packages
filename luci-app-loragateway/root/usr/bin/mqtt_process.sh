#!/bin/sh

logger "MQTT_Process"

#Check If we have set debug
DEBUG=`uci get gateway.general.DEB` 
DEBUG=`echo $DEBUG| awk '{print int($0)}'`

UPDATE_INTERVAL=5

old=`date +%s`

server_type=`uci get mqtt.general.server_type`
QoS=`uci get mqtt.general.QoS`



# Check if the MQTT Server is a fix server
if [ `uci get mqtt.$server_type.fix_server` == "1" ] ;then
	server=`uci get mqtt.$server_type.server`
else
	server=`uci get mqtt.general.server`
fi

#Set MQTT Parameter [-u][-P][-i]
user=`uci get mqtt.general.username`
clientID=`uci get mqtt.general.client_id` 
pass=`uci -q get mqtt.general.password`
port=`uci get mqtt.$server_type.port`
cafile=`uci -q get mqtt.$server_type.ca_file`
pub_format=`uci get mqtt.$server_type.topic_format`
data_format=`uci get mqtt.$server_type.data_format`

# If cafile is not required then set the fields to null
if [[ -z "$cafile" ]];then
	C=" "
	cafile=""
else
	C="--cafile "
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
				#if [ "`uci get mqtt.$channel.local_id`" == "$channel" ]; then
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Find Match Entry for $channel" 
					#Replace Channel ID if we find macro CHANNEL
					remote_id=`uci get mqtt.@channels[$HAS_CID].remote_id`
					topic=`echo ${pub_format/CHANNEL/$remote_id}`
				
					#Replace Channel ID if we find macro WRITE_API
					channel_API=`uci get mqtt.@channels[$HAS_CID].write_api_key`
					topic=`echo ${topic/WRITE_API/$channel_API}`
					
					#Replace username if we find macro USERNAME
					topic=`echo ${topic/USERNAME/$user}`
					
					#Replace clientID if we find macro CLIENTID
					topic=`echo ${topic/CLIENTID/$clientID}`				
					

					#General MQTT Update Data
					data=`cat /var/iot/channels/$channel`
					mqtt_data=`echo ${data_format/DATA/$remote_id $data}`
					
					# Send MQTT Command

# -----------------------------------------
# Debug output just for testing - delete this section when not required
					if [ $DEBUG -ge 1 ]; then
						logger "[IoT.MQTT]:  "
						logger "[IoT.MQTT]:-----"
						logger "[IoT.MQTT]:Parameters"
						logger "[IoT.MQTT]:server[-h]: "$server
						logger "[IoT.MQTT]:port[-p]: "$port
						logger "[IoT.MQTT]:user[-u]: "$user
						logger "[IoT.MQTT]:pass[-P]: "$P$pass
						logger "[IoT.MQTT]:QoS[-q]: "$QoS
						logger "[IoT.MQTT]:clientID[-u]: "$clientID
						logger "[IoT.MQTT]:topic[-t]: "$topic
						logger "[IoT.MQTT]:mqtt_data[-m]: "$mqtt_data
						logger "[IoT.MQTT]:cafile[--cafile]: "$C$cafile
						logger "[IoT.MQTT]:------"
					fi 
# ------------------------------------------

					if [ ! -z "$pass" ]; then  # Check for null password string 
						mosquitto_pub -d -h $server -p $port -u $user -P "$pass" -q $QoS -i $clientID -t $topic -m "$mqtt_data" $C $cafile
					else
						mosquitto_pub -d -h $server -p $port -u $user -q $QoS -i $clientID -t $topic -m "$mqtt_data" $C $cafile
					fi


					### Delete the Channel info
					rm /var/iot/channels/$channel
				else
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Do Not Find Match Entry for $channel" 
					rm /var/iot/channels/$channel
				fi
			done
		fi
	fi
done

