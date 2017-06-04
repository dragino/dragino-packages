#!/bin/sh

# This script creates various config files based on parameters in 
# /etc/config/gpstrack

#get gpswox user_api_hash

[ "$(uci get gpstrack.general.gps_service)" == "disabled" ] && exit 0

username=`uci get gpstrack.gpswox.username`
password=`uci get gpstrack.gpswox.password`
save_username=`uci get gpstrack.gpswox.save_username`
valid_api=`uci get gpstrack.gpswox.valid_api`

if [ "$username" != "$save_username" ] or [ "$valid_api" != "valid" ] ; then
    ret_reqapi=`curl -d "email=$username&password=$password" http://www.gpswox.com/api/login`
    ret_status=`echo "$ret_reapi" | cut -d : -f 2 | cut -d \, -f 1`
    user_api_hash=`echo "$ret_reapi" | cut -d : -f 3 | cut -d \" -f 2`
	
    if [ "$ret_status" = "1" ]; then

        uci set gpstrack.gpswox.user_api_hash=$user_api_hash
        uci set gpstrack.gpswox.save_username=$username
        uci set gpstrack.gpswox.valid_api="valid"

        uci commit gpstrack
    else
        exit 0
    fi
fi

exit 0
