#!/bin/sh

#Auto Update AVR MCU 

HAS_NEW_VERSION=0
#mkdir /tmp/avr
tmp_update_info="/var/avr/update_info"
tmp_image='/var/avr/sketch.hex'
update_log='/var/avr/avrdude_update_log'

mac_identify=`uci get sensor.auto_update.mac_identify`
update_url=`uci get sensor.auto_update.update_url`
update_info=`uci get sensor.auto_update.update_info`
current_ver=`uci get sensor.auto_update.current_ver`

wifi_mac=`ifconfig | grep -m 1 'wlan0' | awk '{gsub(/:/,"")}{print $5}'`
retry_count=3


##Where to get the update info?
if [ $mac_identify = "1" ]; then
	update_info_url="$update_url$wifi_mac.txt"
else 
	update_info_url=$update_url$update_info
fi



# Download Update Information File. This file includes the image used for update, md5sum and the version of the new image. 
[ -f $tmp_update_info ] && rm $tmp_update_info
count=1
while [ $count -le $retry_count ] ;do
	logger "MCU Auto Update: Download Update Information from $update_url (try $count/$retry_count)"
	wget $update_info_url -O $tmp_update_info 2>/var/avr/download_tmp
	result=`cat /var/avr/download_tmp | grep "100%"`
	if [ ! -z "$result" ]; then
		break
	fi 
	count=`expr $count + 1`
	sleep 10
done
	
if [ ! -f $tmp_update_info ]; then
	logger 'MCU Auto Update: Fail to get update info file, Abort'
exit 1
fi
#parse remote info, this will get $image , $md5sum and $version
. $tmp_update_info
if [ -z $image ] || [ -z $md5sum ] || [ -z $version ];then
	logger 'MCU Auto Update: Invalid Update Info File, Abort'
	exit 1
fi

#Check if there is a newer version, 
HAS_NEW_VERSION=`expr $version \> $current_ver`
[ $HAS_NEW_VERSION -eq 0 ] && logger 'MCU Auto Update: The mcu has the latest version already' && exit 0
[ ! $HAS_NEW_VERSION -eq 1 ] && logger 'MCU Auto Update: Version Format Error' && exit 1
logger "Find higher version $version in server, we will download the image $image"

#Download the sketch used for auto update
[ -f $tmp_image ] && rm $tmp_image
wget $update_url/$image -O $tmp_image
if [ ! -f $tmp_image ]; then
	logger "MCU Auto Update: Fail to get sketch $image, Abort"
	echo $image
	exit 1
fi
logger "MCU Auto Update: $image downloaded"

#Check md5sum 
sketch_md5=`md5sum $tmp_image | awk '{print $1}'`
if [ "$sketch_md5" != "$md5sum" ];then
	logger 'MCU Auto Update: md5 checksum fail, Abort'
	exit 1
fi

logger "MCU Auto Update: md5 checksum success. We will flash the image to the MCU"

run-avrdude $tmp_image  > $update_log 2>&1

update_result=`cat $update_log | grep -e 'bytes of flash verified' -e 'Fuses OK'`
if [ -z "$update_result" ]; then
	logger "MCU Auto Update: Programming MCU fail."
	exit 1
else 
	logger "MCU Auto Update: Programming MCU successful, update version number"
	uci set sensor.auto_update.current_ver=$version
	uci commit sensor
fi