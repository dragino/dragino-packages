#!/bin/sh

logger "MQTT_Process"

#Check If we have set debug
DEBUG=`uci get gateway.general.DEB` 
DEBUG=`echo $DEBUG| awk '{print int($0)}'`

CERTPATH="/etc/iot/cert/"
CHAN_FILE="/etc/iot/channels"
KEY_FILE="/etc/lora/devskey"

server_type=`uci get mqtt.common.server_type`
hostname=`uci get system.@[0].hostname`

# Get server specific MQTT parameters
server=`uci -q get mqtt.$server_type.url`
port=`uci -q get mqtt.$server_type.port`
certfile=`uci -q get mqtt.$server_type.cert`
keyfile=`uci -q get mqtt.$server_type.key`
cafile=`uci -q get mqtt.$server_type.cafile`
user=`uci -q get mqtt.$server_type.user`
pass=`uci -q get mqtt.$server_type.pwd`
clientID=`uci -q get mqtt.$server_type.cid` 

pub_qos=`uci -q get mqtt.$server_type.pub_qos`
pub_topic_format=`uci -q get mqtt.$server_type.pub_topic`

data_format=`uci -q get mqtt.$server_type.data`
pub_enable=`uci -q get mqtt.common.pub_enable`

sub_qos=`uci -q get mqtt.$server_type.sub_qos`
sub_topic=`uci -q get mqtt.$server_type.sub_topic`
sub_enable=`uci -q get mqtt.common.sub_enable`

if [ "$pub_enable" != "checked" ];then
  pub_enable=0
fi
if [ "$sub_enable" != "checked" ];then
  sub_enable=0
fi

# Set CA File parameter or set the fields to null
if [[ -z "$cafile" ]];then
	C=" "
	cafile=""
else
	C="--cafile "
	cafile=$CERTPATH$cafile
fi

# Set Certificate and Key file parameters
if [[ -e "$CERTPATH$certfile" ]];then
	cert=$CERTPATH$certfile
else
	cert=""
fi
if [[ -e "$CERTPATH$keyfile" ]];then
	key=$CERTPATH$keyfile
else
	key=""
fi

[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Publish enable:   " $pub_enable
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Subscribe enable: " $sub_enable
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Server:Port       " $server:$port
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Pub Topic Format: " $pub_topic_format
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Pub Data Format:  " $data_format
[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Sub Topic Format: " $sub_topic

# Kill any old Publish / Subscribe copies left running
killall -q "mosquitto_pub"
killall -q "mosquitto_sub"

# ----------------------------------------
# Set up for Subscription

# Check board type and set "radio"
if [ -f /var/iot/board ]; then
	board_type=`cat /var/iot/board`
else
	board_type="LG01"
fi

if [ "$board_type" == "LG01" ]; then
	RADIO="radio1"
	FREQ="RFFREQ"
	SF="RFSF"
	BW="RFBW"
	CR="RFCR"
elif [ "$board_type" == "LG02" ]; then
	RADIO="radio2"
	FREQ="TXFREQ"
	SF="TXSF"
	BW="TXBW"
	CR="TXCR"
fi

LORA=" " # Clear string in case not reqd
if [ "$board_type" == "LG01" ] || [ "$board_type" == "LG02" ]; then
	# Get LoRa radio parameters
	freq=`uci -q get gateway.$RADIO.$FREQ`
	sf=`uci -q get gateway.$RADIO.$SF`
	bw=`uci -q get gateway.$RADIO.$BW`
	cr=`uci -q get gateway.$RADIO.$CR`
	if [ -z "$freq" ] || [ -z "$sf" ] || [ -z "$bw" ] || [ -z "$cr" ] ; then 
		sub_enable=0
		logger "[IoT.MQTT]:Invalid LoRa radio params - mosquitto_sub not called."
	else
		bwstr=125      #set default value
		case $bw in 
			"500000") bwstr="500";;
			"250000") bwstr="250";;
			"125000") bwstr="125";;
			"62500")  bwstr="62.5";;
			"41700")  bwstr="47.8";;
			"31250")  bwstr="31.25";;
			"20800")  bwstr="20.8";;
			"15600")  bwstr="15.6";;
			"10400")  bwstr="10.4";;
			"7800")   bwstr="7.8";;
		esac

		FREQSTR=$(awk "BEGIN {printf \"%.1f\", $freq/1000000}")
		DATR="SF"$sf"BW"$bwstr
		CODR="4/"$cr
		LORA=" -z --freq $FREQSTR --datr $DATR --codr $CODR"
	fi
else
	LORA=" -o "
fi

# Start Subscribe if required

if [ "$sub_enable" != "0" ];then
	# Replace topic macros
	sub_topic=`echo ${sub_topic/CHANNEL/$remote_id}`  
	sub_topic=`echo ${sub_topic/CLIENTID/$clientID}`				
	sub_topic=`echo ${sub_topic/WRITE_API/$channel_API}`
	sub_topic=`echo ${sub_topic/USERNAME/$user}`
	sub_topic=`echo ${sub_topic/HOSTNAME/$hostname}`

	if [ $DEBUG -ge 10 ]; then
		# Set debug flag
		D="-d"
	fi

  # Start the subscription process
	# 1. Case with User, Password and Client ID present
	if [ ! -z "$pass" ] && [ ! -z "$user" ] && [ ! -z "$clientID" ]; then  
		case="1"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos -i $clientID -t $sub_topic -u $user -P "$pass" $C $cafile $LORA &

	# 2. Case with Certificate, Key and ClientID present
	elif [ ! -z "$certfile" ] && [ ! -z "$key" ] && [ ! -z "$clientID" ]; then 
		case="2"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos -i $clientID -t $sub_topic --cert $cert --key $key $C $cafile $LORA &

	# 3. Case with no User, Certificate or ClientID present
	elif [ -z "$user" ] && [ -z "$certfile" ] && [ -z "$clientID" ]; then 
		case="3"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos -t $sub_topic $LORA &

	# 4. Case with no User, no Certificate, but with ClientID present
	elif [ -z "$user" ] && [ -z "$certfile" ] && [ ! -z "$clientID" ]; then
		case="4"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos -i $clientID -t $sub_topic $LORA &

	# 5. Case with User and ClientID present, but no Password and no Certificate present
	elif [ -z "$pass" ] && [ -z "$certfile" ] && [ ! -z "$user" ] && [ ! -z "$clientID" ]; then  
		case="5"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos -i $clientID -t $sub_topic -u $user $LORA &

	# 6. Case with User and Password present, but no ClientID and no Certificate present
	elif [ ! -z "$user" ] && [ ! -z "$pass" ] && [ -z "$clientID" ] && [ -z "$certfile" ]; then
		case="6"  
		mosquitto_sub $D -h $server -p $port -q $sub_qos  -t $sub_topic -u $user -P "$pass"

	# 0. Else - invalid parameters, just log
	else
		case="Invalid parameters"  
		logger "[IoT.MQTT]:Invalid Parameters - mosquitto_sub not called."
	fi

	msubcount=$(ps | grep -c mosquitto_sub)
	if [ "$msubcount" == "2" ]; then
		subflag="1"
		echo "Subscribe Running." > /var/iot/status
	fi 
	fping -q $server
	if [ $? -eq "0" ]; then
		echo " Online" >> /var/iot/status
	else
		echo " Offline" >> /var/iot/status
	fi


	if [ $DEBUG -ge 10 ]; then
		# Echo parameters to console
		echo " "
		echo "Board Type: "$board_type
		echo "MQTT Subscribe Case: "$case
		echo "-----"
		echo "MQTT Subscribe Parameters"
		echo "server: "$server
		echo "port: "$port
		echo "user: "$user
		echo "pass: "$pass
		echo "sub_qos: "$sub_qos
		echo "cert: "$cert
		echo "key: "$key
		echo "cafile: "$cafile
		echo "clientID: "$clientID
		echo "sub_topic: "$sub_topic
		echo "sub_enable: "$sub_enable
		echo "LORA: "$LORA
		echo "------"
	fi

fi

# -----------------------------------
# Set up for Publish

# Set up channels directory
mkdir -p /var/iot/channels

# Check if Publish is enabled, exit if not
if [ "$pub_enable" == "0" ]; then
	if [ $DEBUG -ge 10 ]; then
		echo "Publish not enabled. Exit"
	fi
	exit # Nothing more to do
else
	# Status
	if [ "$subflag" == "1" ]; then
		echo "Pub, Sub" > /var/iot/status
	else
		echo "Publish" > /var/iot/status
	fi
	echo " Enabled." >> /var/iot/status
	fping -q $server
	if [ $? -eq "0" ]; then
		echo " Online" >> /var/iot/status
	else
		echo " Offline" >> /var/iot/status
	fi
fi

if [ $DEBUG -ge 10 ]; then
	# Echo parameters to console
	echo " "
	echo "-----"
	echo "Starting Publish process - waiting for input"
	echo " "
	echo "-----"
	echo "MQTT Publish Parameters"
	echo "server: "$server
	echo "port: "$port
	echo "user: "$user
	echo "pass: "$pass
	echo "pub_qos: "$pub_qos
	echo "certfile: "$certfile
	echo "cert: "$cert
	echo "key: "$key
	echo "cafile: "$cafile
	echo "clientID: "$clientID
	echo "------"
fi

# Run Forever - process publish requests.
while [ 1 ]
do
	inotifywait -q -e 'create,modify' /var/iot/channels/
	
		CID=`ls /var/iot/channels/`
		[ $DEBUG -ge 2 ] && logger "[IoT.MQTT]: Check for sensor update"
		if [ -n "$CID" ];then
			[ $DEBUG -ge 2 ] && [ -n "$CID" ] && logger "[IoT.MQTT]: Found Data at Local Channels:" $CID
			for channel in $CID; do
				CHAN_INFO=`sqlite3 $CHAN_FILE "SELECT *from mapping where local = '$channel';"`
				if [ -n "$CHAN_INFO" ];then
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Find Match Entry for $channel" 

					# Get values
					local_id=`echo $CHAN_INFO | awk -F '\\|' '{print $1}'`
					remote_id=`echo $CHAN_INFO | awk -F '\\|' '{print $2}'`
					channel_API=`echo $CHAN_INFO | awk -F '\\|' '{print $3}'`
					
					# Send Data  
					# META 					
					meta=`cat /var/iot/channels/$channel`
					time=`echo $meta | cut -d , -f 1` 
					rssi=`echo $meta | cut -d , -f 2` 	
					# Strip out meta data if not required
					data=$(echo $meta | cut  -d , -f 3-100)		# For late version with metadata from radio
 					#data=$(cat /var/iot/channels/$channel)   # For early version with no metadata from radio
					
 					# JSON 
					str=`echo $data | sed 's/{//'` 
					json="{\"timestamp\":\"$time\",\"rssi\":\"$rssi\",$str" 
					
					# Generate Topic and Data strings
					# Initialise strings
					pub_topic=$pub_topic_format
					mqtt_data=$data_format

					# Replace topic macros
					pub_topic=`echo ${pub_topic/CHANNEL/$remote_id}`  
					pub_topic=`echo ${pub_topic/CLIENTID/$clientID}`				
					pub_topic=`echo ${pub_topic/WRITE_API/$channel_API}`
					pub_topic=`echo ${pub_topic/USERNAME/$user}`
					pub_topic=`echo ${pub_topic/HOSTNAME/$hostname}`
					# Replace data macros
					mqtt_data=`echo ${mqtt_data/HOSTNAME/$hostname}`  
					mqtt_data=`echo ${mqtt_data/CHANNEL/$remote_id}`
					mqtt_data=`echo ${mqtt_data/DATA/$data}`  
					mqtt_data=`echo ${mqtt_data/META/$meta}`  
					mqtt_data=`echo ${mqtt_data/JSON/$json}`  
					
					PUB_FLAG="-m "  # Default
					DECODER=`sqlite3 $KEY_FILE "SELECT decoder from abpdevs where devaddr = '$channel';"`					
					logger "[IoT.MQTT]: DECODER $DECODER $channel"
					# Send the File
					if [ ! -z $DECODER ]; then
						PUB_FLAG="-m "
						if [ "$DECODER" == "ASCII" ]; then
							#Send As ASCII String
							rssi=`hexdump -v -e '11/1 "%c"'  -n 16 /var/iot/channels/$channel | tr A-Z a-z`
							payload=`xxd -p /var/iot/channels/$channel`
							payload=`echo ${payload:32}`
							mqtt_data=$rssi$payload
						else
							#Decode the sensor value use pre-set format and send
							mqtt_data=`/etc/lora/decoder/$DECODER $channel`
						fi
					elif [ "$data_format" == "FILE" ];then 
						PUB_FLAG="-f"
						mqtt_data="/var/iot/channels/$channel"	
						
					elif [ "$data_format" == "BIN_ASCII" ];then
						xxd -p /var/iot/channels/$channel | tr -d '\n'> /var/iot/channels/$channel.asc
						PUB_FLAG="-m "
						mqtt_data=$(cat /var/iot/channels/$channel.asc)
					else 
						DECODER="Not Set"
					fi

					# Initialise debug flag
					D=" "
					if [ $DEBUG -ge 10 ]; then
						# Set test level debug flag
						D="-d"
					fi

					# ------------------------------------------
					# Call MQTT Publish command
					
					# 1. Case with User, Password and Client ID present  (e.g. Azure)
					if [ ! -z "$pass" ] && [ ! -z "$user" ] && [ ! -z "$clientID" ]; then
						case="1"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos -i $clientID -t $pub_topic -u $user -P "$pass" $C $cafile $PUB_FLAG "$mqtt_data"
						
					# 2. Case with Certificate, Key and ClientID present (e.g. AWS)
					elif [ ! -z "$certfile" ] && [ ! -z "$key" ] && [ ! -z "$clientID" ]; then
						case="2"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos -i $clientID -t $pub_topic --cert $cert --key $key $C $cafile $PUB_FLAG "$mqtt_data"
						
					# 3. Case with no User, Certificate or ClientID present
					elif [ -z "$user" ] && [ -z "$certfile" ] && [ -z "$clientID" ]; then
						case="3"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos -t $pub_topic $PUB_FLAG "$mqtt_data" 
						
					# 4. Case with no User, Certificate, but with ClientID present
					elif [ -z "$user" ] && [ -z "$certfile" ] && [ ! -z "$clientID" ]; then
						case="4"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos -i $clientID -t $pub_topic $PUB_FLAG "$mqtt_data"
						
					# 5. Case with User and ClientID present, but no Password and no Certificate present
					elif [ -z "$pass" ] && [ -z "$certfile" ] && [ ! -z "$user" ] && [ ! -z "$clientID" ]; then
						case="5"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos -i $clientID -t $pub_topic -u $user $PUB_FLAG "$mqtt_data"
						
					# 6. Case with User and Password present, but no ClientID and no Certificate present
					elif [ ! -z "$user" ] && [ ! -z "$pass" ] && [ -z "$clientID" ] && [ -z "$certfile" ]; then
						case="6"  
						mosquitto_pub $D -h $server -p $port -q $pub_qos  -t $pub_topic -u $user -P "$pass" $PUB_FLAG "$mqtt_data"
						
					# 0. Else - invalid parameters, just log
					else
						case="Invalid parameters"  
						logger "[IoT.MQTT]:Invalid Parameters - mosquitto_pub not called."
					fi

					# -----------------------------------------
					# Debug output to log
					if [ $DEBUG -ge 1 ]; then
						logger "[IoT.MQTT]:  "
						logger "[IoT.MQTT]:-----"
						logger "[IoT.MQTT]:MQTT Publish Case: "$case
						logger "[IoT.MQTT]:MQTT Publish Parameters"
						logger "[IoT.MQTT]:server[-h]: "$server
						logger "[IoT.MQTT]:port[-p]: "$port
						logger "[IoT.MQTT]:user[-u]: "$user
						logger "[IoT.MQTT]:pass[-P]: "$pass
						logger "[IoT.MQTT]:pub_qos[-q]: "$pub_qos
						logger "[IoT.MQTT]:cafile[--cafile]: "$cafile
						logger "[IoT.MQTT]:cert[--cert]: "$cert
						logger "[IoT.MQTT]:key[--key]: "$key
						logger "[IoT.MQTT]:clientID[-i]: "$clientID
						logger "[IoT.MQTT]:remote_id: "$remote_id
						logger "[IoT.MQTT]:pub_topic[-t]: "$pub_topic
						logger "[IoT.MQTT]:decoder: "$DECODER
						if [ "$data_format" == "FILE" ];then
							logger "[IoT.MQTT]:mqtt_file[-f]: $mqtt_data"
						else
							logger "[IoT.MQTT]:mqtt_data[-m]: $mqtt_data"
						fi	
						logger "[IoT.MQTT]:------"
					fi 

					# ------------------------------------------
					# Debug console output for manual testing

					if [ $DEBUG -ge 10 ]; then
						# Echo parameters to console
						echo " "
						echo "-----"
						echo "MQTT Publish Case: "$case
						echo "MQTT Publish Parameters"
						echo "server: "$server
						echo "port: "$port
						echo "user: "$user
						echo "pass: "$pass
						echo "pub_qos: "$pub_qos
						echo "certfile: "$certfile
						echo "cert: "$cert
						echo "key: "$key
						echo "cafile: "$cafile
						echo "clientID: "$clientID
						echo "remoteID: "$remote_id
						echo "pub_topic: "$pub_topic
						echo "decoder: "$DECODER
						if [ "$data_format" == "FILE" ];then 
							echo "[IoT.MQTT]:mqtt_file[-f]: $mqtt_data"
						else
							echo "[IoT.MQTT]:mqtt_data[-m]: $mqtt_data"
						fi
						echo "------"
					fi

					# ------------------------------------------
					# Delete the Channel info
					rm /var/iot/channels/$channel*
					
				else
					[ $DEBUG -ge 1 ] && logger "[IoT.MQTT]: " "Did Not Find Match Entry for $channel" 
					rm /var/iot/channels/$channel.*
				fi
			done
		fi
done

