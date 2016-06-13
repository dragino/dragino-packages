#!/bin/sh


PKG_PID=`ps | grep "auto_provisioning" | grep -v grep | awk '{print $1}'`
SELF_PID=$$
if [ ! -z "$PKG_PID" ];then
	for pid in $PKG_PID;do 
		if [ $pid != $SELF_PID ]; then
			kill -s 9 $pid
		fi 
	done
fi

HAS_NEW_VERSION=0
RETRY_COUNT=3
tmp_dir='/var/auto_update'
tmp_provisioning_file="/var/auto_update/provisioning_file"

[ ! -d $tmp_dir ] && mkdir $tmp_dir

mac_identify=`uci get provisioning.auto_update.mac_identify`
update_url=`uci get provisioning.auto_update.update_url`
update_file_name=`uci get provisioning.auto_update.update_file_name`
current_ver=`uci get provisioning.auto_update.current_ver`
check_internet_host=`uci get provisioning.auto_update.check_internet_host`
update_protocol=`uci get provisioning.auto_update.update_protocol`

eth0_mac=`ifconfig | grep -m 1 'eth0' | awk '{gsub(/:/,"")}{print tolower($5)}'`


##Whether to check if there is internet connection before stat the update
if [ ! -z "$check_internet_host" ]; then
	#Check_host defined. process update only when there is network connection to Check Host.
	#MAX_RETRY_COUNT is to avoid the infinite loop if a wrong host is provided. 
	MAX_RETRY_COUNT=20
	MAX_WARN=5
	CUR_COUNT=1
	while [ -z "`fping -e $check_internet_host | grep alive`" ] && [ $CUR_COUNT -le $MAX_RETRY_COUNT ]
	do
		if [ $CUR_COUNT -le $MAX_WARN ];then
			logger '[Autoprovisioning System]: No net connection to check host'
		fi
		CUR_COUNT=`expr $CUR_COUNT + 1`
		echo $CUR_COUNT $MAX_RETRY_COUNT
		sleep 10
	done
fi

##Where to get the update info?
if [ $mac_identify = "1" ]; then
	update_file_url="$update_url/$eth0_mac.conf"
else 
	update_file_url=$update_url/$update_file_name
fi

# Download Autoprovisioning File. 
[ -f $tmp_provisioning_file ] && rm $tmp_provisioning_file
count=1
while [ $count -le $RETRY_COUNT ] ;do
	logger "[Autoprovisioning System]: Downloading $update_file_url (try $count/$RETRY_COUNT)"
	if [ $update_protocol = 'http' ] || [ $update_protocol = 'https' ];then
		wget $update_file_url -O $tmp_provisioning_file 2> $tmp_dir/update_log
		result=`cat $tmp_dir/update_log | grep "100%"`
	elif [ $update_protocol = 'tftp' ];then
		atftp -g -r $eth0_mac.conf -l $tmp_provisioning_file  $update_url --trace 2>$tmp_dir/update_log
		tftp_result=`cat $tmp_dir/update_log | grep -c "tftp: aborting"`
		if [ $tftp_result -eq 0 ];then
			result=1
		fi
	fi 
	if [ ! -z "$result" ]; then
		break
	fi 
	count=`expr $count + 1`
	sleep 10
done
	
if [ ! -f $tmp_provisioning_file ]; then
	logger '[Autoprovisioning System]: Fail to get update info file, Abort'
	exit 1
fi

#parse file
. $tmp_provisioning_file
if [ -z $version ];then
	logger '[Autoprovisioning System]: Invalid Autoprovisioning File, Abort'
	exit 1
fi

#Check if there is a newer version
HAS_NEW_VERSION=`expr $version \> $current_ver`
[ $HAS_NEW_VERSION -eq 0 ] && logger '[Autoprovisioning System]: The device has the latest version already' && exit 0
[ ! $HAS_NEW_VERSION -eq 1 ] && logger '[Autoprovisioning System]: Version Format Error' && exit 1
logger "[Autoprovisioning System]: Find higher version $version in server, update config"

update_network_section(){
	#check if there is update version
	if [ $internet = 'wan' ];then
		#switch to WAN
		uci set secn.wan.wanport='Ethernet'
		uci set secn.wan.ethwanmode='dhcp'
	elif [ $internet = 'cellular' ];then
		#switch to USB-Modem
		uci set secn.wan.wanport='USB-Modem'
		uci set secn.wan.ethwanmode=''
	fi
	uci commit secn
	/usr/bin/config_secn
}

update_voip_section(){
	# MAX Server Accounts and Max dial rule for each account
	MAXACCOUNTS=5
	MAXRULE=10
	
	#Parse VoIP Info
	#Delete Current Server Account Info
	servers=`uci show voip | grep 'server\[[0-9]\]=server' | awk -F '[][]' '{print $2}'`
	for server in $servers; do
		uci delete voip.@server[0]
	done
	#Configure servers account from provioning file, only support 5 voip accounts now
	for i in $(seq 1 1 $MAXACCOUNTS);do
		if  [ ! -z $(eval echo '$'voip${i}_host) ];then
			entry=`uci add voip server`
			uci set voip.$entry.register=$(eval echo '$'voip${i}_enable)
			uci set voip.$entry.protocol=$(eval echo '$'voip${i}_protocol)
			uci set voip.$entry.host=$(eval echo '$'voip${i}_host)
			uci set voip.$entry.username=$(eval echo '$'voip${i}_user)
			uci set voip.$entry.secret=$(eval echo '$'voip${i}_password)
			uci set voip.$entry.phonenumber=$(eval echo '$'voip${i}_phonenumber)
		fi
	done
	uci commit voip

	#Parse Dial Rules
	#Delete Current Dial-Rules
	rules=`uci show dial-rules | grep 'rule\[[0-9]\]=rule' | awk -F '[][]' '{print $2}'`
	for rule in ${rules}; do
		uci delete dial-rules.@rule[0]
	done
	#Configure dial rules from provioning file, only support 10 dial rules  now
	for i in $(seq 1 1 $MAXACCOUNTS);do
		for j in $(seq 1 1 $MAXRULE);do
			if  [ ! -z $(eval echo '$'voip${i}_rule${j}_pattern) ];then
				entry=`uci add dial-rules rule`
				uci set dial-rules.$entry.pattern=$(eval echo '$'voip${i}_rule${j}_pattern)
				uci set dial-rules.$entry.prefix=$(eval echo '$'voip${i}_rule${j}_prefix)
				uci set dial-rules.$entry.offset=$(eval echo '$'voip${i}_rule${j}_offset)
				uci set dial-rules.$entry.length=$(eval echo '$'voip${i}_rule${j}_length)
				uci set dial-rules.$entry.suffix=$(eval echo '$'voip${i}_rule${j}_suffix)
				uci set dial-rules.$entry.callerid=$(eval echo '$'voip${i}_rule${j}_callerid)
				uci set dial-rules.$entry.protocol=$(eval echo '$'voip${i}_rule${j}_protocol)
				if [ -z $(eval echo '$'voip${i}_rule${j}_trunk) ];then
					uci set dial-rules.$entry.trunk=$(eval echo '$'voip${i}_user)
				else
					uci set dial-rules.$entry.trunk=$(eval echo '$'voip${i}_rule${j}_trunk)
				fi	
			fi
		done
	done
	uci commit dial-rules
	/usr/bin/config2asterisk
}

update_pkg_maintain_section(){
	echo '' > /etc/opkg.conf
cat >> /etc/opkg.conf << EOF
dest root /
dest ram /tmp
lists_dir ext /var/opkg-lists
option overlay_root /overlay
arch all 10
arch ar71xx 100
EOF
#-----------------------------------------------------------

for i in $(seq 1 1 10);do
		if  [ ! -z $(eval echo '$'package_link${i}) ];then
			echo "src/gz package_source_$i $(eval echo '$'package_link${i})" >> /etc/opkg.conf
		fi
done

if [ ! -z $(eval echo '$'selected_packages) ];then
	uci set provisioning.package_info.selected_packages=$selected_packages
fi

uci commit provisioning

}

execute_post_command(){
	for i in $(seq 1 1 10);do
		if  [ ! -z $(eval echo '$'post_command{i}) ];then
			if [ "$(eval echo '$'post_command${i})" = "reboot" ];then
				reboot
			fi
		fi
	done
}

update_general_section(){
	##Update Provioning Settings
	if [ ! -z "$mac_file" ]; then
		uci set provisioning.auto_update.mac_identify=$mac_file
	fi

	if [ ! -z "$new_url" ]; then
		uci set provisioning.auto_update.update_url=$new_url
	fi

	if [ ! -z "$new_file" ]; then
		uci set provisioning.auto_update.update_file_name=$new_file
	fi
	
	if [ ! -z "$check_new_host" ]; then
		uci set provisioning.auto_update.check_internet_host=$check_new_host
	fi

	uci set provisioning.auto_update.current_ver=$version
	uci commit provisioning
}

update_log_server_section(){
	##Update Logging Server section
	if [ ! -z "$log_server" ]; then
		uci set system.log.log_server=$log_server
	fi
	if [ ! -z "$log_protocol" ]; then
		uci set system.log.log_protocol=$log_protocol
	fi
}

if [ "$update_network" = "1" ]; then
	update_network_section
fi 

if [ "$update_voip" = "1" ]; then
	update_voip_section
fi

if [ "$update_pkg_maintain" = "1" ]; then
	update_pkg_maintain_section
fi

if [ "$update_log_server" = "1" ]; then
	update_log_server_section
fi

update_general_section

if [ "$run_post_command" = "1" ]; then
	execute_post_command
fi
