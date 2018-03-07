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

m = Map("lorawan", translate("LoRa Gateway Settings"), translate("Configuration to communicate with LoRa devices and LoRaWAN server"))

s = m:section(NamedSection, "general", "lorawan", translate("LoRaWAN Server Settings"))
local sv = s:option(Value, "server", translate("Server Address"))
sv.datatype = "host"
sv.placeholder = "Domain or IP"

local sp = s:option(Value, "port", translate("Server Port"))
sp.datatype = "port"
sp.default = "1700"


local gid = s:option(Value, "gateway_id", translate("Gateway ID"))
gid.placeholder = "Gateway ID from Server"


local mail = s:option(Value, "mail", translate("Mail Address"))
mail.placeholder = "Mail address sent to Server"

local lati = s:option(Value, "lati", translate("Latitude"))
lati.placeholder = "Location Info"

local long = s:option(Value, "long", translate("Longtitude"))
long.placeholder = "Location Info"

s = m:section(NamedSection, "radio", "lorawan", translate("Radio Settings"),translate("Radio settings requires MCU side sketch support"))
local tx_fre = s:option(Value, "tx_frequency", translate("TX Frequency"),translate("Gateway's LoRa TX Frequency"))
tx_fre.placeholder = "9 digits Frequency, etc:868100000"
tx_fre.datatype = "rangelength(9,9)"

local rx_fre = s:option(Value, "rx_frequency", translate("RX Frequency"),translate("Gateway's LoRa RX Frequency"))
rx_fre.placeholder = "9 digits Frequency, etc:868100000"
rx_fre.datatype = "rangelength(9,9)"

local encry = s:option(Value, "encryption", translate("Encryption Key"))
encry.placeholder = "Encryption Key"

local sf = s:option(ListValue, "SF", translate("Spreading Factor"))
sf.placeholder = "Spreading Factor"
sf.default = "7"
sf:value("6", "SF6")
sf:value("7", "SF7")
sf:value("8", "SF8")
sf:value("9", "SF9")
sf:value("10", "SF10")
sf:value("11", "SF11")
sf:value("12", "SF12")

local codr = s:option(ListValue, "coderate", translate("Coding Rate"))
codr.placeholder = "Coding Rate"
codr.default = "5"
codr:value("5", "4/5")
codr:value("6", "4/6")
codr:value("7", "4/7")
codr:value("8", "4/8")

local sbw = s:option(ListValue, "BW", translate("Signal Bandwidth"))
sbw.placeholder = "Signal Bandwidth"
sbw.default = "7"
sbw:value("0", "7.8 kHz")
sbw:value("1", "10.4 kHz")
sbw:value("2", "15.6 kHz")
sbw:value("3", "20.8 kHz")
sbw:value("4", "31.25 kHz")
sbw:value("5", "41.7 kHz")
sbw:value("6", "62.5 kHz")
sbw:value("7", "125 kHz")
sbw:value("8", "250 kHz")
sbw:value("9", "500 kHz")

local prb = s:option(Value, "preamble", translate("Preamble Length"), translate("Length range: 6 ~ 65536"))
prb.placeholder = "6 ~ 65536"
prb.default = "8"

return m
