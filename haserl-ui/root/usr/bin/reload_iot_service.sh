#!/bin/sh

service=`uci get gateway.general.server_type`

#kill possible running process. Loriot, Station
echo 1
killall -q mosquitto_sub             # Remove any remaining MQTT subscribe process
killall -q loriot_dragino_lg308_spi  # Remove any remaining LORIOT process
killall -q tcp_process
#ps | grep "lg02_pkt_fwd" | grep -v grep | awk '{print $1}' | xargs kill -s 9
killall -q station
#cd /etc/station ; station -k
rm -f /var/iot/status

/etc/init.d/iot reload

killall -q inotifywait                  # Remove any inotfywait process, need to put at the last

