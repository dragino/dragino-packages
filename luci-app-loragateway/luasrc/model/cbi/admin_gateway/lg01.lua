--[[
LuCI - Lua Configuration Interface

Copyright 2017 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id: LoRaWAN.lua 5948 2010-03-27 14:54:06Z jow $
]]--

local uci = luci.model.uci.cursor()
local mac="a84041efefef"

m = Map("gateway", translate("Single Channel LoRa Gateway"), translate("Configuration to communicate with LoRa devices and LoRaWAN server"))

s = m:section(NamedSection, "general", "lorawan", translate("LoRaWAN Server Settings"))

local sv = s:option(ListValue, "server_type", translate("IoT Service"))
sv.placeholder = "Select IoT service"
sv.default = "disabled"
sv:value("disabled",  "Disabled") 
sv:value("lorawan",  "LoRaWan/RAW forwarder")        
sv:value("mqtt",  "LoRaRAW forward to MQTT server")
sv:value("tcpudp",  "LoRaRAW forward to TCP/UDP server")
sv:value("customized",  "Process LoRa Data via customized script")

local lv = s:option(ListValue, "DEB", translate("Debug Level"))
lv.placeholder = "Select debug level"
lv.default = "1"
lv:value("0",  "No debug")
lv:value("1",  "Little message output")
lv:value("2",  "More verbose output")
lv:value("3",  "Many verbose output")

local sp = s:option(ListValue, "provider", translate("Service Provider"))
sp.default = "ttn"
sp:value("ttn", "The Things Network")
sp:value("tencent", "Tencent LoRaWAN Server")
sp:value("local", "Built-In LoRaWAN Server")
sp:value("custom", "--custom--")

local sv = s:option(Value, "custom_server", translate("Server Address"))
sv.datatype = "host"
sv.placeholder = "Domain or IP"
sv:depends("provider","custom")

local ttn_addr = s:option(ListValue, "ttn_server", translate("Server Address"))
ttn_addr:depends("provider","ttn")
ttn_addr.default = "router.eu.thethings.network"
ttn_addr:value("router.eu.thethings.network", "ttn-router-eu")
ttn_addr:value("router.us.thethings.network", "ttn-router-us-west")
ttn_addr:value("router.as.thethings.network", "ttn-router-asia-se")
ttn_addr:value("router.as1.thethings.network", "ttn-router-asia-se-1")
ttn_addr:value("router.as2.thethings.network", "ttn-router-asia-se-2")
ttn_addr:value("router.kr.thethings.network", "router.kr.thethings.network")
ttn_addr:value("router.jp.thethings.network", "router.jp.thethings.network")
ttn_addr:value("thethings.meshed.com.au", "ttn-router-asia-se")
ttn_addr:value("as923.thethings.meshed.com.au", "meshed-router")
ttn_addr:value("ttn.opennetworkinfrastructure.org", "switch-router")
ttn_addr:value("router.cn.thethings.network", "router.cn.thethings.network")

local tencent_addr = s:option(ListValue, "tencent_server", translate("Server Address"))
tencent_addr:depends("provider","tencent")
tencent_addr.default = "cn.thethings.network"
tencent_addr:value("cn.thethings.network", "cn.thethings.network")

local local_addr = s:option(ListValue, "local_server",translate("Server Address"))
local_addr:depends("provider","local")
local_addr.default = "127.0.0.1"
local_addr:value("127.0.0.1", "127.0.0.1")


local sp = s:option(Value, "port", translate("Server upstream Port"))
sp.datatype = "port"
sp.default = "1700"

local dport = s:option(Value, "dwport", translate("Server downstream Port"))
dport.datatype = "port"
dport.default = "1700"

local gid = s:option(Value, "GWID", translate("Gateway ID"))
gid.placeholder = "Gateway ID from Server"
gid.default=mac

local mail = s:option(Value, "email", translate("Mail Address"))
mail.placeholder = "Mail address sent to Server"

local lati = s:option(Value, "LAT", translate("Latitude"))
lati.placeholder = "Location Info"

local long = s:option(Value, "LON", translate("Longtitude"))
long.placeholder = "Location Info"

local rf_power = s:option(Value, "RFPOWER", translate("Radio Power (Unit:dBm)"))
rf_power.placeholder = "range 5 ~ 20 dBm"
rf_power.datatype = "rangelength(1,2)"
rf_power.default = "20"

s = m:section(NamedSection, "radio1", "lorawan", translate("Radio Settings"),translate("Radio settings for Channel"))
local rf_fre = s:option(Value, "RFFREQ", translate("Frequency (Unit:Hz)"))
rf_fre.placeholder = "9 digits Frequency, etc:868100000"
rf_fre.datatype = "rangelength(9,9)"

local rf_sf = s:option(ListValue, "RFSF", translate("Spreading Factor"))
rf_sf.placeholder = "Spreading Factor"
rf_sf.default = "7"
rf_sf:value("6", "SF6")
rf_sf:value("7", "SF7")
rf_sf:value("8", "SF8")
rf_sf:value("9", "SF9")
rf_sf:value("10", "SF10")
rf_sf:value("11", "SF11")
rf_sf:value("12", "SF12")

local codr = s:option(ListValue, "RFCR", translate("Coding Rate"))
codr.placeholder = "Coding Rate"
codr.default = "5"
codr:value("5", "4/5")
codr:value("6", "4/6")
codr:value("7", "4/7")
codr:value("8", "4/8")

local sbw = s:option(ListValue, "RFBW", translate("Signal Bandwidth"))
sbw.placeholder = "Signal Bandwidth"
sbw.default = "125000"
sbw:value("7800", "7.8 kHz")
sbw:value("10400", "10.4 kHz")
sbw:value("15600", "15.6 kHz")
sbw:value("20800", "20.8 kHz")
sbw:value("31250", "31.25 kHz")
sbw:value("41700", "41.7 kHz")
sbw:value("62500", "62.5 kHz")
sbw:value("125000", "125 kHz")
sbw:value("250000", "250 kHz")
sbw:value("500000", "500 kHz")

local prb = s:option(Value, "RFPRLEN", translate("Preamble Length"), translate("Length range: 6 ~ 65536"))
prb.placeholder = "6 ~ 65536"
prb.default = "8"

local syncwd = s:option(Value, "SYNCWD", translate("LoRa Sync Word"), translate("Value 52(0x34) for LoRaWAN"))
syncwd.placeholder = "Value 52(0x34) is reserved for LoRaWAN networks"
syncwd.default = "52"

--local encry = s:option(Value, "encryption", translate("Encryption Key"))
--encry.placeholder = "Encryption Key"

return m
