#!/bin/sh
. /usr/share/libubox/jshn.sh

cur_version=`uci get system.@system[0].cfg_version`

#usb block mountpoint which contain upgrade image
mountpoint=`uci get system.@system[0].mountpoint` 
cfg_path="/mnt/${mountpoint}"

GWID=`xxd -ps -l 6 -s 0x1002 /dev/mtd6`
CFG="$GWID.json"

IMAGE="dragino-lgw-squashfs-sysupgrade.bin"

cfg_version=""
cfg_server=""
gr_gwcfg=""
gr_conn_type=""
gr_serv_addr=""
my_gwcfg=""
my_conn_type=""
my_serv_addr=""

#################################################
#         
#     
#    
#  "gatewayid.json": {
#         "general": {
#                       "system": {"cfg_version":"1.0", "cfg_server":"server"}
#           },
#          "gateway": {
#                       "general": {"gwcfg": "EU"},
#                       "server1": {"connection_type": "legacy", "server_address": "route.au.thetings.network"}
#           }
#   }
#         
#     
#    
#################################################

read_cfg() {
    local _str
    json_load_file "$1"
    json_select general
        json_select system
            json_get_var cfg_version cfg_version
            json_get_var cfg_server cfg_server
        json_select ..
    json_select ..

    json_select gateway
        json_select general
            json_get_var gr_gwcfg gwcfg
        json_select ..
        json_select server1
            json_get_var gr_conn_type connection_type
            json_get_var gr_serv_addr server_address
        json_select ..
    json_select ..

    # if GWID object in config file
    _str=`grep ${GWID} $1`
    [ -n ${_str} ] && {
        json_select ${GWID}
            json_select general
                json_get_var my_gwcfg gwcfg
            json_select ..
            json_select server1
                json_get_var my_conn_type connection_type
                json_get_var my_serv_addr server_address
            json_select ..
        json_select ..
    }
    json_cleanup
    [ -z "${my_gwcfg}" ] && my_gwcfg="${gr_gwcfg}"
    [ -z "${my_conn_type}" ] && my_conn_type="${gr_conn_type}"
    [ -z "${my_serv_addr}" ] && my_serv_addr="${gr_serv_addr}"
}

set_cfg() {
    local _var=""
    [ -n "${my_gwcfg}" ] && uci set gateway.general.gwcfg="${my_gwcfg}" && _var="set"
    [ -n "${my_conn_type}" ] && uci set gateway.server1.connection_type="${my_conn_type}" && _var="set"
    [ -n "${my_serv_addr}" ] && uci set gateway.server1.server_address="${my_serv_addr}" && _var="set"
    [ -n "${_var}" ] && uci commit gateway && generate-config.sh && logger -t auto_update "auto update gateway configure"
}

if [ -f ${cfg_path}/${CFG} ]; then
    cd ${cfg_path}
    read_cfg ${CFG}
else
# download config file form cfgserver
    [ -n "${cfg_server}" ] && {
        alive=`fping ${cfg_server} | grep -c alive`
        [ -n "${alive}" ] && cd /tmp && wget http://${cfg_server}/${CFG}
        [ -f ${CFG} ] && logger -t auto_update "Get update config form ${cfg_server}" && read_cfg ${CFG}
    }
fi

[ ! -f /tmp/${CFG} ] && [ ! -f ${cfg_path}/${CFG} ] && logger -t auto_update "configure file not found"

set_cfg

[ -f ${cfg_path}/${IMAGE} ] && {
    cd ${cfg_path} 
    sysupgrade ${IMAGE}
}
