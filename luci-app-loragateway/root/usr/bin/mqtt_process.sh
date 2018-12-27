#!/bin/sh

logger "MQTT_Process"

#Check If we have set debug
DEBUG=`uci get gateway.general.DEB` 
DEBUG=`echo $DEBUG| awk '{print int($0)}'`

UPDATE_INTERVAL=5


old=`date +%s`

server_type=`uci get mqtt.general.server_type`

# Check if the MQTT Server is pre_config
if [ `uci get mqtt.$server_type` == "general" ];then
	pre_config_server=0
else
	pre_config_server=1
fi

#Set MQTT Server Address [-h]
if [ $pre_config_server -eq 0 ];then
	server=`uci get mqtt.general.server`
else 
	server=`uci get mqtt.$server_type.server`
fi

#Set MQTT Server Port [-p]
if [ $pre_config_server -eq 0 ];then
	port=`uci get mqtt.general.port`
else 
	port=`uci get mqtt.$server_type.port`
fi

#Set MQTT Parameter [-u][-P][-i]
user=`uci get mqtt.general.username`
pass=`uci get mqtt.general.password`
clientID=`uci get mqtt.general.client_id`


#Set MQTT Publish topic format
if [ $pre_config_server -eq 0 ];then
	pub_format=`uci get mqtt.general.topic_format`
else 
	pub_format=`uci get mqtt.$server_type.topic_format`
fi

#Set MQTT Publish data format
if [ $pre_config_server -eq 0 ];then
	data_format=`uci get mqtt.general.data_format`
else 
	data_format=`uci get mqtt.$server_type.data_format`
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
					
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "[-t] $topic"	

					#General MQTT Update Data
					data=`cat /var/iot/channels/$channel`
					mqtt_data=`echo ${data_format/DATA/$data}`
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "[-m] $mqtt_data"
					
					### Send MQTT Command
					mosquitto_pub -h $server -p $port -u $user -P $pass  -i $clientID -t $topic -m $mqtt_data
					
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

