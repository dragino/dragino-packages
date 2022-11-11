#!/bin/sh

file=`uci get customized_script.general.para1`
logger "[IoT]: Store Sensor Data to "

CHECK_INTERVAL=5


old=`date +%s`

#Run Forever
while [ 1 ]
do
	now=`date +%s`
	
	# Check if there is sensor data arrive at /var/iot/channels/ every 5 seconds
	if [ `expr $now - $old` -gt $CHECK_INTERVAL ];then
		old=`date +%s`
		CID=`ls /var/iot/channels/`
		if [ -n "$CID" ];then
			for channel in $CID; do
				data=`cat /var/iot/channels/$channel`			
				logger "[IoT]: Found $data at Local Channel:" $CID
				
				logger "[IoT]: Append at $file"
				echo `date` ":<$channel> $data" >> $file
				rm /var/iot/channels/$channel
			done
		fi
	fi
done