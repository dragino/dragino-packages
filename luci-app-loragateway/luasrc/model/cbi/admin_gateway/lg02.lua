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

m = Map("gateway", translate("LoRa Gateway Settings"), translate("Configuration to communicate with LoRa devices and LoRaWAN server"))

s = m:section(NamedSection, "general", "lorawan", translate("LoRaWAN Server Settings"))

local sp = s:option(ListValue, "provider", translate("Service Provider"))
sp.default = "ttn"
sp:value("ttn", "The Things Network")
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

local sp = s:option(Value, "port", translate("Server Port"))
sp.datatype = "port"
sp.default = "1700"

local gid = s:option(Value, "GWID", translate("Gateway ID"))
gid.placeholder = "Gateway ID from Server"
gid.default=mac

local mail = s:option(Value, "email", translate("Mail Address"))
mail.placeholder = "Mail address sent to Server"

local lati = s:option(Value, "LAT", translate("Latitude"))
lati.placeholder = "Location Info"

local long = s:option(Value, "LON", translate("Longtitude"))
long.placeholder = "Location Info"

local mode = s:option(ListValue, "mode", translate("RadioMode"))
mode.placeholder = "Radio Mode"
mode.default = "0"
mode:value("0", "A for RX, B for TX")
mode:value("1", "B for RX, A for TX")
mode:value("2", "Both for RX")

s = m:section(NamedSection, "radio1", "lorawan", translate("Channel 1 Radio Settings"),translate("Radio settings for Channel 1"))
local rx_fre = s:option(Value, "RXFREQ", translate("RX Frequency (Unit:Hz)"))
rx_fre.placeholder = "9 digits Frequency, etc:868100000"
rx_fre.datatype = "rangelength(9,9)"

local rx_sf = s:option(ListValue, "RXSF", translate("RX Spreading Factor"))
rx_sf.placeholder = "Spreading Factor"
rx_sf.default = "7"
rx_sf:value("6", "SF6")
rx_sf:value("7", "SF7")
rx_sf:value("8", "SF8")
rx_sf:value("9", "SF9")
rx_sf:value("10", "SF10")
rx_sf:value("11", "SF11")
rx_sf:value("12", "SF12")

local tx_fre = s:option(Value, "TXFREQ", translate("TX Frequency (Unit:Hz)"))
tx_fre.placeholder = "9 digits Frequency, etc:868100000"
tx_fre.datatype = "rangelength(9,9)"

local tx_sf = s:option(ListValue, "TXSF", translate("TX Spreading Factor"))
tx_sf.placeholder = "Spreading Factor"
tx_sf.default = "9"
tx_sf:value("6", "SF6")
tx_sf:value("7", "SF7")
tx_sf:value("8", "SF8")
tx_sf:value("9", "SF9")
tx_sf:value("10", "SF10")
tx_sf:value("11", "SF11")
tx_sf:value("12", "SF12")

local codr = s:option(ListValue, "RXCR", translate("Coding Rate"))
codr.placeholder = "Coding Rate"
codr.default = "5"
codr:value("5", "4/5")
codr:value("6", "4/6")
codr:value("7", "4/7")
codr:value("8", "4/8")

local sbw = s:option(ListValue, "RXBW", translate("Signal Bandwidth"))
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

local prb = s:option(Value, "RXPRLEN", translate("Preamble Length"), translate("Length range: 6 ~ 65536"))
prb.placeholder = "6 ~ 65536"
prb.default = "8"

local syncwd = s:option(Value, "syncwd", translate("LoRa Sync Word"))
syncwd.placeholder = "Value 52(0x34) is reserved for LoRaWAN networks"
syncwd.default = "52"

local encry = s:option(Value, "encryption", translate("Encryption Key"))
encry.placeholder = "Encryption Key"

s = m:section(NamedSection, "radio2", "lorawan", translate("Channel 2 Radio Settings"),translate("Radio settings for Channel 2"))
local tx_fre = s:option(Value, "TXFREQ", translate("TX Frequency (Unit:Hz)"))
tx_fre.placeholder = "9 digits Frequency, etc:868100000"
tx_fre.datatype = "rangelength(9,9)"

local rx_sf = s:option(ListValue, "TXSF", translate("TX Spreading Factor"))
rx_sf.placeholder = "Spreading Factor"
rx_sf.default = "7"
rx_sf:value("6", "SF6")
rx_sf:value("7", "SF7")
rx_sf:value("8", "SF8")
rx_sf:value("9", "SF9")
rx_sf:value("10", "SF10")
rx_sf:value("11", "SF11")
rx_sf:value("12", "SF12")

local rx_fre = s:option(Value, "RXFREQ", translate("RX Frequency (Unit:Hz)"))
rx_fre.placeholder = "9 digits Frequency, etc:868100000"
rx_fre.datatype = "rangelength(9,9)"

local tx_sf = s:option(ListValue, "RXSF", translate("RX Spreading Factor"))
tx_sf.placeholder = "Spreading Factor"
tx_sf.default = "9"
tx_sf:value("6", "SF6")
tx_sf:value("7", "SF7")
tx_sf:value("8", "SF8")
tx_sf:value("9", "SF9")
tx_sf:value("10", "SF10")
tx_sf:value("11", "SF11")
tx_sf:value("12", "SF12")

local codr = s:option(ListValue, "TXCR", translate("Coding Rate"))
codr.placeholder = "Coding Rate"
codr.default = "5"
codr:value("5", "4/5")
codr:value("6", "4/6")
codr:value("7", "4/7")
codr:value("8", "4/8")

local sbw = s:option(ListValue, "TXBW", translate("Signal Bandwidth"))
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

local prb = s:option(Value, "TXPRLEN", translate("Preamble Length"), translate("Length range: 6 ~ 65536"))
prb.placeholder = "6 ~ 65536"
prb.default = "8"

local syncwd = s:option(Value, "syncwd", translate("LoRa Sync Word"))
syncwd.placeholder = "Value 52(0x34) is reserved for LoRaWAN networks"
syncwd.default = "52"

local encry = s:option(Value, "encryption", translate("Encryption Key"))
encry.placeholder = "Encryption Key"

return m
