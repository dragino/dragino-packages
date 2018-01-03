#!/bin/sh

/etc/init.d/lg08 stop

sleep 1

server=`uci get gateway.general.serverl`
upp=`uci get gateway.general.upp`
dwp=`uci get gateway.general.dwp`
gwid=`uci get gateway.general.gwid`
gwcfg=`uci get gateway.general.gwcfg`

if [[ ! -z ${gwcfg} ]]; then
    cp -rf /etc/lora/cfg/global_conf.json.${gwcfg} /etc/lora/global_conf.json
fi

if [[ ! -z ${gwid} ]]; then
    if [ ${#gwid} -ne 16 ]; then
        GWID_MIDFIX="FFFE"
        GWID_BEGIN=$(cat /sys/class/net/eth0/address | awk -F\: '{print $1$2$3}')
        GWID_END=$(cat /sys/class/net/eth0/address | awk -F\: '{print $4$5$6}')
        sed -i 's/\(^\s*"gateway_ID":\s*"\).\{16\}"\s*\(,\?\).*$/\1'${GWID_BEGIN}${GWID_MIDFIX}${GWID_END}'"\2/' /etc/lora/local_conf.json
    fi
    sed -i 's/\(^\s*"gateway_ID":\s*"\).\{16\}"\s*\(,\?\).*$/\1'${gwid}'"\2/' /etc/lora/local_conf.json
fi

if [[ ! -z ${server} ]]; then
    sed -i -i 's/\(^\s*"server_address":\s*"\).\{16\}"\s*\(,\?\).*$/\1'${server}'"\2/' /etc/lora/global_conf.json
fi

if [[ ! -z ${upp} ]]; then
    sed -i -i 's/\(^\s*"serv_port_up":\s*"\).\{16\}"\s*\(,\?\).*$/\1'${upp}'"\2/' /etc/lora/global_conf.json
fi

if [[ ! -z ${dwp} ]]; then
    sed -i -i 's/\(^\s*"serv_port_down":\s*"\).\{16\}"\s*\(,\?\).*$/\1'${dwp}'"\2/' /etc/lora/global_conf.json
fi

sleep 1

/etc/init.d/lg08 start


