#!/bin/sh
. /usr/share/libubox/jshn.sh

def_cfg=`uci get gateway.general.gwcfg`
subband=`uci get gateway.general.subband`
gwid=`uci get gateway.general.GWID`
provider=`uci get gateway.general.provider`
server=`uci get gateway.general.${provider}_server`
upp=`uci get gateway.general.port`
dpp=`uci get gateway.general.dwport`
stat=`uci get gateway.general.stat`
email=`uci get gateway.general.email`
maccrypto=`uci get gateway.general.maccrypto`
mac2db=`uci get gateway.general.mac2db`
custom_downlink=`uci get gateway.general.custom_downlink`

latitude=`uci get gateway.general.LAT`
longitude=`uci get gateway.general.LON`
altitude=`uci get gateway.general.ALT`
fake_gps=`uci get gateway.general.fake_gps`

model=`cat /tmp/iot/model.txt`
json_section_name="SX130x_conf"


gen_gw_cfg() {
 
	json_init
    json_add_object gateway_conf
	json_add_string "platform" "SX1$chip"
	json_add_string "description" "Dragino LoRaWAN Gateway"	
	json_add_string "email" "$email"
	json_add_string "gateway_ID" "$gwid" 
	
	json_add_string "log_mask" "100"    # Log Level
	json_add_boolean "radiostream_enable" 1    # Enable SX Radio TX /RX
	
	#ghoststream
	json_add_boolean "ghoststream_enable" 0    # Enable virtual radio TX/RX
	json_add_string "ghost_host" "localhost"
	json_add_int "ghost_port" "1760" 
	
	#LBT Settings 
	json_add_boolean "lbt_enable" 0
	json_add_boolean "lbt_isftdi" 1

	#Remote manager
	json_add_boolean "manage_enable" 0
	json_add_string	"manage_host" "host"
	json_add_int "manage_port" "1990"

	#Enable Database Configure, if enable , sqlitedb settings will overwrite globalconf settings. 
	json_add_boolean "dbconf_enable" 1
	

	json_add_boolean "wd_enable" 1  	#Watchdog
	
	#ABP Communication
	if [ "$maccrypto" = "1" ];then
		json_add_boolean "mac_decode" 1	    #ABP Decode
	fi

	json_add_boolean "mac2file" 1   # Save Decode Payload to file 

	if [ "$mac2db" = "1" ];then
		json_add_boolean "mac2db" 1	    
    else
		json_add_boolean "mac2db" 0	   
	fi

	if [ "$custom_downlink" = "1" ];then
		json_add_boolean "custom_downlink" 1	    
    else
		json_add_boolean "custom_downlink" 0	    
	fi

        # stastatic 状态和统计的间隔时间，单位是秒*/
	json_add_int	"stat_interval" "30"  # 这个和 server 里面的 Kepp Alive 区别? 

	##GPS coordinates. use fake GPS from UI or via GPS module
	if [ "$fake_gps" == "1" ];then
		json_add_boolean "fake_gps" 1
		json_add_double "ref_latitude" "$latitude"
		json_add_double "ref_longitude" "$longitude"
		json_add_double "ref_altitude" "$altitude"	
	elif [ "$model" == "DLOS8" ];then	
		json_add_string "gps_tty_path" "/dev/ttyUSB0"
	fi
     
 

	###Beacon Related
	json_add_int "beacon_period" "0"
	json_add_int "beacon_freq_hz" "869525000"
	json_add_int "beacon_datarate" "9"
	json_add_int "beacon_bw_hz" "125000"	
	json_add_int "beacon_power" "14"
	json_add_int "beacon_infodesc" "0"	

	###Servers Setting. 
	json_add_array "servers"
	
		#Server1
		json_add_object "server1"
		
		json_add_string "server_name" "`uci get gateway.server1.server_id`"  #name是服务的标识，必须要设置一个name   ?? 哪里需要用到，可以用 Server 1 替代吗？
		json_add_string "server_type" "semtech"  #服务类型有：semtech, mqtt, gwtraft, ttn	几个服务类型什么区别，什么场合用到. MQTT 是指 LoRaWAN over MQTT 吧? 
		json_add_string	"server_id"  "`uci get gateway.server1.mqtt_user`" # 类型是mqtt或ttn时才需要设置   --> 对应哪个参数?? 连接例子? 
		json_add_string "server_key" "`uci get gateway.server1.mqtt_pass`" # 类型是mqtt或ttn时才需要设置	--> 对应哪个参数?? 连接例子?
		json_add_string	"server_address" "`uci get gateway.server1.server_address`"   
		json_add_int "serv_port_up" "`uci get gateway.server1.upp`"
		json_add_int "serv_port_down" "`uci get gateway.server1.dpp`"
		
		#adjust the following parameters for your network 
		json_add_int "keepalive_interval" "`uci get gateway.server1.keepalive_interval`" #以前的默认值是多少? 
		json_add_int "push_timeout_ms" "`uci get gateway.server1.push_timeout_ms`"  #以前的默认值是多少? 
		json_add_int "pull_timeout_ms" "`uci get gateway.server1.pull_timeout_ms`" #以前的默认值是多少?

		#forward only valid packets
		  #                   /*fport的过滤方法, 0是不处理，1是只转发数据库里设置了的fport，2不转发数据库里的fport */
		  #                   /*fport的数据库的key是 /filter/server_name/fport/fport_num, value可以是yes、no */
		  #                   /*devaddr:  /filter/server_name/devaddr/devaddr/yes,例如:filter/name/devaddr/112233111/yes */
		json_add_int "fport_filter" "`uci get gateway.server1.fport_filter`" 
		json_add_int "filter_fport" "`uci get gateway.server1.devaddr_filter`"
		json_add_string "devaddr_filter" "`uci get gateway.server1.devaddr_filter`"
		json_add_boolean "forward_crc_valid" "`uci get gateway.server1.forward_crc_valid`"
		json_add_boolean "forward_crc_error" "`uci get gateway.server1.forward_crc_error`"
		json_add_boolean "forward_crc_disabled" "`uci get gateway.server1.forward_crc_disabled`"
		json_close_object 
	
		#Server2
		json_add_object "server1"
		json_add_string "server_name" "name"  #name是服务的标识，必须要设置一个name   ?? 哪里需要用到，可以用 Server 1 替代吗？
		json_add_string "server_type" "semtech"  #服务类型有：semtech, mqtt, gwtraft, ttn	几个服务类型什么区别，什么场合用到. MQTT 是指 LoRaWAN over MQTT 吧? 
		json_add_string	"server_id"  "mqtt_user" # 类型是mqtt或ttn时才需要设置   --> 对应哪个参数?? 连接例子? 
		json_add_string "server_key" "mqtt_pass" # 类型是mqtt或ttn时才需要设置	--> 对应哪个参数?? 连接例子?
		json_add_string	"server_address" "localhost"   
		json_add_int "serv_port_up" "1730"
		json_add_int "serv_port_down" "1730"
		
		#adjust the following parameters for your network 
		json_add_int "keepalive_interval" "10" #以前的默认值是多少? 
		json_add_int "push_timeout_ms" "100"  #以前的默认值是多少? 
		json_add_int "pull_timeout_ms" "100" #以前的默认值是多少?

		#forward only valid packets
		  #                   /*fport的过滤方法, 0是不处理，1是只转发数据库里设置了的fport，2不转发数据库里的fport */
		  #                   /*fport的数据库的key是 /filter/server_name/fport/fport_num, value可以是yes、no */
		  #                   /*devaddr:  /filter/server_name/devaddr/devaddr/yes,例如:filter/name/devaddr/112233111/yes */
		json_add_int "fport_filter" "0" 
		json_add_int "filter_devaddr" "0" 
		json_add_string "devaddr_filter" "0"
		json_add_boolean "forward_crc_valid" 1
		json_add_boolean "forward_crc_error" 0
		json_add_boolean "forward_crc_disabled" 0
		json_close_object 
		
	json_close_array

    json_close_object
    json_dump  > /etc/lora/local_conf.json
}

gen_cus_cfg() {
	json_init
	json_add_object SX130x_conf
		json_add_string "spidev_path" "/dev/spidev0.0"
		json_add_boolean "full_duplex" 0
		json_add_boolean "lorawan_public" 1
		json_add_int "clksrc" "1"
		json_add_string "clksrc_desc" "radio_1 provides clock to concentrator"
		json_add_int "antenna_gain" "0"
		json_add_string "antenna_gain_desc" "antenna gain, in dBi"
		
		json_add_object radio_0
			json_add_boolean "enable" "`uci get gateway.general.radio0_enable`"
			json_add_string "type" "SX1257"
			radio0_freq="`uci get gateway.general.radio0_freq`"
			json_add_int "freq" "$radio0_freq"
			json_add_int "rssi_offset" "-166.0"
			json_add_boolean "tx_enable" "`uci get gateway.general.radio0_tx`"
			json_add_int "tx_freq_min" "`uci get gateway.general.radio0_txfreq_min`"
			json_add_int "tx_freq_max" "`uci get gateway.general.radio0_txfreq_max`"
		json_close_object

		json_add_object radio_1
			json_add_boolean "enable" "`uci get gateway.general.radio1_enable`"
			json_add_string "type" "SX1257"
			radio1_freq="`uci get gateway.general.radio1_freq`"
			json_add_int "freq" "$radio1_freq"
			json_add_int "rssi_offset" "-166.0"
			json_add_boolean "tx_enable" "`uci get gateway.general.radio1_tx`"
			json_add_int "tx_freq_min" "`uci get gateway.general.radio1_txfreq_min`"
			json_add_int "tx_freq_max" "`uci get gateway.general.radio1_txfreq_max`"
		json_close_object


		for i in `seq 0 7`
		do
			json_add_object chan_multiSF_$i
				json_add_boolean "enable" "`uci get gateway.general.chan${i}_enable`"
				chan_radio="`uci get gateway.general.chan${i}_radio`"
				json_add_int "radio" "$chan_radio"
				chan_if="`uci get gateway.general.chan$i`"
				json_add_int "if" "$chan_if"
				if [ "${chan_radio}" = "0" ]; then
					json_add_string "desc" "Lora MAC, 125kHz, all SF, ${radio0_freq}, if: ${chan_if}"
				elif [ "${chan_radio}" = "1" ]; then
					json_add_string "desc" "Lora MAC, 125kHz, all SF, ${radio1_freq}, if: ${chan_if}"
				fi
			json_close_object
		done
		
		json_add_object chan_Lora_std
			json_add_boolean "enable" "`uci get gateway.general.lorachan_enable`"
			chan_radio="`uci get gateway.general.lorachan_radio`"
			json_add_int "radio" "$chan_radio"
			chan_if="`uci get gateway.general.lorachan`"
			json_add_int "if" "$chan_if"
			chan_band="`uci get gateway.general.lorachan_bw`"
			json_add_int "bandwidth" "$chan_band"
			chan_sf="`uci get gateway.general.lorachan_sf`"
			json_add_int "spread_factor" "$chan_sf"
			if [ "$chan_radio" = "0" ]; then
				json_add_string "desc" "Lora MAC, SF=$chan_sf, bandwidth=$chan_band, $radio0_freq, if: $chan_if"
			elif [ "$chan_radio" = "1" ]; then
				json_add_string "desc" "Lora MAC, SF=$chan_sf, bandwidth=$chan_band, $radio1_freq, if: $chan_if"
			fi
		json_close_object
		
		json_add_object chan_FSK
			json_add_boolean "enable" 0
			json_add_string "desc" "Disabled"
		json_close_object

		json_add_object tx_lut_0
			json_add_string "desc" "TX gain table, index 0"
			json_add_int "pa_gain" "2"
			json_add_int "mix_gain" "11"
			json_add_int "rf_power" "12"
			json_add_int "dig_gain" "2"
		json_close_object

		json_add_object tx_lut_1
			json_add_string "desc" "TX gain table, index 1"
			json_add_int "pa_gain" "2"
			json_add_int "mix_gain" "14"
			json_add_int "rf_power" "17"
			json_add_int "dig_gain" "1"
		json_close_object	

		json_add_object tx_lut_2
			json_add_string "desc" "TX gain table, index 2"
			json_add_int "pa_gain" "2"
			json_add_int "mix_gain" "15"
			json_add_int "rf_power" "20"
			json_add_int "dig_gain" "1"
		json_close_object	
		
		json_add_object tx_lut_3
			json_add_string "desc" "TX gain table, index 3"
			json_add_int "pa_gain" "3"
			json_add_int "mix_gain" "14"
			json_add_int "rf_power" "27"
			json_add_int "dig_gain" "0"
		json_close_object
	json_close_object
	
	json_add_object gateway_conf
		json_add_string "server_address" "router.eu.thethings.network"
		json_add_int "serv_port_up" "1700"
		json_add_int "serv_port_down" "1700"
		json_add_array "servers"
			json_add_object "server1"
				json_add_string "server_address" "router.eu.thethings.network"
				json_add_int "serv_port_up" "1700"
				json_add_int "serv_port_down" "1700"
				json_add_boolean "serv_enabled" 1
			json_close_object
		json_close_array
	json_close_object 
	json_dump  > /etc/lora/global_conf.json	
}

echo_chan_if() {
    echo "Gateway Channels frequency" > /etc/lora/desc
    echo "---------------------------------------" >> /etc/lora/desc

    json_load_file "/etc/lora/global_conf.json"
    json_select $json_section_name
    for i in `seq 0 7`
    do
        json_select chan_multiSF_$i
        json_get_var desc desc
        json_select ..
        echo "chan_multSF_$i" >> /etc/lora/desc 
        echo "$desc" >> /etc/lora/desc
        echo "---------------------------------------" >> /etc/lora/desc
    done

    json_select chan_Lora_std
    json_get_var desc desc
    json_select ..
    echo "chan_Lora_std" >> /etc/lora/desc
    echo "$desc" >> /etc/lora/desc
    echo "---------------------------------------" >> /etc/lora/desc

    json_select chan_FSK
    json_get_var desc desc
    json_select ..
    echo "chan_FSK" >> /etc/lora/desc
    echo "$desc" >> /etc/lora/desc
    echo "---------------------------------------" >> /etc/lora/desc
	
    json_cleanup

}

if [ "$def_cfg" = "AU" ] || [ "$def_cfg" = "US" ] || [ "$def_cfg" = "CN" ];then
	def_cfg="$def_cfg-$subband"
fi


if [ $model == "LG308" ] || [ $model == "DLOS8" ];then
	chip="301"
elif [ $model == "LPS8" ];then
	chip="308"
elif [ $model == "LIG16" ];then
	chip="302"
	json_section_name="SX130x_conf"
fi


######### Generate LoRaWAN Settings   #########
gen_gw_cfg # Generate local_conf.json

if [ -f /etc/lora/cfg-$chip/"$def_cfg"-global_conf.json ] 
then 
    cp -rf /etc/lora/cfg-$chip/"$def_cfg"-global_conf.json /etc/lora/global_conf.json
elif [ "$def_cfg" = "CUS" ];then
logger "Customized Freq"
	gen_cus_cfg
else 
    cp -rf /etc/lora/cfg-$chip/EU-global_conf.json /etc/lora/global_conf.json
fi

echo_chan_if  # Show the used frequency
