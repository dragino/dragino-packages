#!/bin/sh /etc/rc.common
START=99

start()
{	
	manual_connect=$(uci get rssh.rssh.manual_connect)
	
	if [ ! -f "/etc/rc.d/S99DR_enable_rssh.sh" ] && [ $manual_connect != "1" ];then
		exit
	fi

	# Check for successful connection
	sleep 2
	check_connect=$(ps | grep "ssh" | grep "localhost:22" -c)
	
	# Connecr to Server
	if [ $check_connect != "1" ]; then
		host_id="$(uci -q get rssh.rssh.host_id)"
		host_addr="$(uci -q get rssh.rssh.host_addr)"
		rssh_id="$(uci -q get rssh.rssh.rssh_id)"
		killall rssh_client
		killall ssh
		rssh_client -s $host_addr -m $rssh_id -u $host_id > /dev/null &
	fi
}

stop()
{	
	echo "Manual disconnect" > /tmp/date.txt
	uci set rssh.rssh.manual_connect="0"
	uci commit rssh
	killall rssh_client
	killall ssh
}