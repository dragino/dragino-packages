#!/bin/sh
# Script to process http connection. 

# Check if SSL is enable
if [ `uci -q get http_iot.general.SSL` == "1" ];then
	secure="-k"
else
	secure=""
fi

# Get LoRa Parameters;
board=`cat /var/iot/board`
if [[ "$board" = "LG02" ]]; then
	freq=`uci -q get gateway.radio2.TXFREQ`
	sf=`uci -q get gateway.radio2.TXSF`
	cr=`uci -q get gateway.radio2.TXCR`
	bandwidth=`uci -q get gateway.radio2.TXBW`
elif [[ "$board" = "LG01" ]]; then
	freq=`uci -q get gateway.radio1.RFFREQ`
	sf=`uci -q get gateway.radio1.RFSF`
	cr=`uci -q get gateway.radio1.RFCR`
	bandwidth=`uci -q get gateway.radio1.RFBW`
fi

let freqb=$freq/1000000
let freqs=$freq%1000000

# function to parse JSON 
parse_json(){
echo "${1//\"/}" | sed "s/.*$2:\([^,}]*\).*/\1/"
}

#"

# Get downstream polling interval
down_interval=`uci -q get http_iot.general.down_interval`
down_interval=`expr $down_interval + 0`

down_url=`uci -q get http_iot.general.down_url`
down_para=`uci -q get http_iot.general.down_para`
down_type=`uci -q get http_iot.general.down_type`

old=`date +%s`

#Run Forever 

while [ 1 ]
do 
	now=`date +%s`
	
	##Process upstream
	#if [ `uci -q get http_iot.general.up_enable` = "1" ];then
	#fi

	
	##Process downstream
	if [ `uci -q get http_iot.general.down_enable` == "1" ];then
		if [ `expr $now - $old` -gt $down_interval ];then
			old=`date +%s`
			down_raw=`curl $secure -s $down_url` 

			len=`echo $down_raw |wc -L`

			# downstream must be longer than 2 bytes. 
			if [ $len -gt 2 ]; then
				logger "[IoT.Downlink:] $down_raw"
				value=$(parse_json $down_raw $down_para)
				down_text="{\"txpk\":{\"freq\":$freqb.$freqs,\"powe\":20,\"datr\":\"SF${sf}BW$bandwidth\",\"codr\":\"4/$cr\",\"ipol\":false,\"data\":\"$value\"}}"
				echo $down_text > /var/iot/push/test
			fi
		fi
  else
    sleep 1  # Wait before looping to check time
	fi 
done


