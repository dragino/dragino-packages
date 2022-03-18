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
	check_connect=$(ps | grep "ssh" | grep "yfNR" -c)
	
	# Connecr to Server
	if [ $check_connect != "1" ] || [ $manual_connect == "1" ]; then
		host_id="$(uci -q get rssh.rssh.host_id)"
		host_addr="$(uci -q get rssh.rssh.host_addr)"
		rssh_id="$(uci -q get rssh.rssh.rssh_id)"
		killall rssh_client
		killall ssh
		type="$(uci -q get rssh.rssh.connection_check)"
		
		if [ $type == "1" ]; then
		rssh_client -s $host_addr -m $rssh_id -u $host_id -i /etc/dropbear/id_dropbear > /dev/null &		
		else
		rssh_client -s $host_addr -m $rssh_id -u $host_id  > /dev/null &
		fi
		[ $manual_connect == "1" ] && uci set rssh.rssh.manual_connect="0";	
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