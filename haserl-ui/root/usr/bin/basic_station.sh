#!/bin/bash

. /usr/share/libubox/jshn.sh

chip="302"

fport_filter_level=$(uci get gateway.station.fport_filter_level)
devaddr_filter_level=$(uci get gateway.station.devaddr_filter_level)
nwkid_filter_level=$(uci get gateway.station.nwkid_filter_level)
deveui_filter_level=$(uci get gateway.station.deveui_filter_level)

if [[ -z $fport_filter_level ]]; then
	fport_filter_level=0
fi

if [[ -z $devaddr_filter_level ]]; then
	devaddr_filter_level=0
fi

if [[ -z $nwkid_filter_level ]]; then
	nwkid_filter_level=0
fi

if [[ -z $deveui_filter_level ]]; then
	deveui_filter_level=0
fi

gen_bs_cfg() {
	json_init
    	json_add_object station
			json_add_string "server_name" "server"
			json_add_int "fport_filter" "$fport_filter_level" 
			json_add_int "devaddr_filter" "$devaddr_filter_level"
			json_add_int "nwkid_filter" "$nwkid_filter_level"
			json_add_int "deveui_filter" "$deveui_filter_level"
		json_close_object

	json_dump  > /etc/station/local_conf.json.tmp
	cp /etc/station/local_conf.json.tmp  /etc/station/local_conf.json
}

case "$1" in
	start)
		gen_bs_cfg # Generate local_conf.json
		;;
	*)
esac

exit
