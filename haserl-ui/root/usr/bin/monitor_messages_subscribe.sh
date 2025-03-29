#!/bin/bash

# Retrieve configuration values
HOSTNAME=$(uci get system.@system[0].hostname)
Gateway_EUI=$(uci -q get gateway.general.GWID)
BROKER="lns1.thingseye.io"
SUB_TOPIC="dragino/gateway/down/$HOSTNAME"
PUB_TOPIC="dragino/gateway/status/$HOSTNAME"
PORT=8883
CAFILE='/tmp/ca.pem' # Certificate path
MAC=$(hexdump -v -s $((0x1000)) -n 10 /dev/mtd6 | awk '{print $3 $4 $5}')

# Define acknowledgment messages
ack_data="{\"Hostname\":\"$HOSTNAME\",\"Gateway_EUI\":\"$Gateway_EUI\",\"status\":\"ACK\"}"
ack_logging="{\"Hostname\":\"$HOSTNAME\",\"Gateway_EUI\":\"$Gateway_EUI\",\"status\":\"Log_is_being_recorded.\"}"
ack_log="{\"Hostname\":\"$HOSTNAME\",\"Gateway_EUI\":\"$Gateway_EUI\",\"status\":\"Logs_have_been_sent\"}"

# Subscribe to MQTT topic and process incoming messages
mosquitto_sub -h $BROKER -p $PORT -t $SUB_TOPIC --cafile $CAFILE | while read -r line; do
    # Check if the line contains an action
    if echo "$line" | grep -q '"action":'; then
        # Extract the action value
        action_start=$(echo "$line" | awk -F'"action":"' '{print $2}' | awk -F'"' '{print $1}')
        
        # Process the action
        case "$action_start" in
            "auto-update")
                rm /tmp/.first_boot_updated
				/etc/init.d/opkg-update start
                mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$ack_data"
                ;;
            "package_info")
                dragino_gw_fwd_version=$(opkg list_installed | grep -e 'dragino_gw_fwd' | awk '{print $3}')
                haserl_ui_version=$(opkg list_installed | grep -e 'haserl-ui' | awk '{print $3}')
                lg02_pkt_fwd_version=$(opkg list_installed | grep -e 'lg02_pkt_fwd' | awk '{print $3}')
                lora_gateway_version=$(opkg list_installed | grep -e 'lora-gateway' | awk '{print $3}')

                upload_data="{\"Hostname\":\"$HOSTNAME\",\"Gateway_EUI\":\"$Gateway_EUI\",\"dragino_gw_fwd\":\"$dragino_gw_fwd_version\",\"haserl_ui\":\"$haserl_ui_version\",\"lg02_pkt_fwd\":\"$lg02_pkt_fwd_version\",\"lora_gateway\":\"$lora_gateway_version\"}"
                mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$upload_data"
                /usr/bin/monitor_gateway.sh
                ;;
            "set_server1_address")
                server_address=$(echo "$line" | awk -F'"server_address":"' '{print $2}' | awk -F'"' '{print $1}')

                if [[ -n "$server_address" ]]; then
                    echo "Setting server address to $server_address"
                    uci set gateway.server1.provider='custom'
                    uci set gateway.server1.server_address="$server_address"
                    uci commit gateway

                    /etc/init.d/lora_gw reload > /dev/null
                    sleep 2
                    mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$ack_data"
                else
                    echo "Invalid server address: $server_address"
                fi
                ;;
            "reboot")
                mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$ack_data"
                reboot
                ;;
            "get_latest_log_file")
                echo "get_latest_log_file"
                mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$ack_logging"
                touch /tmp/logging.flag

                # Determine server type and collect logs
                if [[ "$server_type" == "station" ]]; then
                    killall tail
                    cat /var/iot/station.log >> /tmp/logfile.log
                    tail -f /var/iot/station.log >> /tmp/logfile.log &
                else
                    killall logread
                    echo "DMESG:" > /tmp/logfile.log
                    dmesg >> /tmp/logfile.log
                    echo "LOGREAD:" >> /tmp/logfile.log
                    logread >> /tmp/logfile.log
                    echo "LOGREAD -f:" >> /tmp/logfile.log
                    /usr/bin/save_log.sh &
                fi

                sleep 300
                rm /tmp/logging.flag
                killall logread

                # Upload the log file
                curl -u "$MAC:dragino" -X PUT --upload-file /tmp/logfile.log "https://lns1.thingseye.io/upload/$MAC/logfile.log"
                mosquitto_pub -h $BROKER -p $PORT -t $PUB_TOPIC --cafile $CAFILE -m "$ack_log"
                ;;
            *)
                echo "Unknown action: $action_start"
                ;;
        esac
    fi
done