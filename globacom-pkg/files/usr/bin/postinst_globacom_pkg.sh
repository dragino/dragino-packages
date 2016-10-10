#POST Install for Globacom Package
patch_322(){
	uci set firewall.@forwarding[0].dest=lan
	uci commit firewall
	/etc/init.d/firewall restart
}

patch_324(){
	[ -z "`cat /etc/crontabs/root | grep "reset_device"`" ] && echo "" >> /etc/crontabs/root && echo "00 03 * * * /usr/bin/reset_device" >> /etc/crontabs/root
	/etc/init.d/cron restart
}

patch_330(){
	rm /lib/modules/3.10.49/dragino2_si3217x.ko
	mv /etc/dragino2_si3217x /lib/modules/3.10.49/dragino2_si3217x.ko
}

restart_service(){
	/usr/bin/glo_routine &
	/etc/init.d/asterisk stop
	/etc/init.d/dragino2-si3217x stop
	sleep 20
	/etc/init.d/dragino2-si3217x start
	/etc/init.d/asterisk start
}

patch_322
patch_324
patch_330
restart_service