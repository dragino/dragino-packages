#!/bin/sh

# format :  user_api_hash
#           fixTime
#           valid
#           unqueID
#           latitude
#           longitude
#           attributes
#           speed
#           altitude
#           course
#           protocol

# http protocol: get

# Example: http://107.170.92.234/insert.php?user_api_hash=8033a8484a2eb7067e943310fd87cf15&fixTime=1494917145000&valid=1&uniqueId=401056&latitude=22.895882&longitude=114.511006&attributes=%7B%22battery%22:%220%22%7D&speed=0&altitude=47.5175&course=0&protocol=iOS

# ip address  IP : 109.235.68.205 (Europe)
#             IP : 107.170.92.234 (USA)
#             IP : 128.199.127.94 (Asia)

## generate rand number for uniqueid

function rand(){
    min=$1
    max=$(($2-$min+1))
    num=$(($RANDOM+1000000000))
    return $(($num%$max+$min))  
}

##

##### latitude #########
function getlat() {
    return "22.895882"
}

##### longitude #########
function getlng() {
    return "114.511006"
}

##### altitude ##########
function getalt() {
    return "47.5175"
}

## gpswox tracking server
server=`uci get gpstrack.gpswox.server`

case $server in 
    1) ip="109.235.68.205"
        ;;
    2) ip="107.170.92.234"
        ;;
    *) ip="128.199.127.94"
        ;;
esac

user_api_hash=`uci get gpstrack.pgswox.user_api_hash`

function send_data() {

    attributes="battery:100"

    speed=0

    course=0

    fixTime=`date +%s`"000"

    uniqueId=rand(100000, 999999) 

    latitude=getlat()

    longitude=getlng()

    altitude=getalt()

    data="user_api_hash=$user_api_hash&fixTime=$fixTime&valid=1&uniqueId=$uniqueId&latitude=$latitude&longitude=$longitude&attributes=$attributes&speed=$speed&altitude=$altitude&course=$course&protocol=iOS"

    req="http://$ip/insert.php"

    curl -d $data $req

    return 0
}

let fre=`uci get gpstrack.gpswox.frequency`

if [ $fre -lt 60 ]; then
    frequency=`expr 60 / $fre`
    sl=`expr 60 / $frequency`
    for ((i=1; i<=$frequency; i++)); do
        send_data()
        sleep sl
    done
else
    send_data()
fi

exit 0
