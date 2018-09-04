--[[
LuCI - Lua Configuration Interface

Copyright 2017 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id: LoRaWAN.lua 5948 2010-03-27 14:54:06Z jow $
]]--

local m, s, o

m = Map("gateway", translate("LoRa Gateway Settings"), translate("Configuration to communicate with LoRa devices and LoRaWAN server"))

m:chain("luci")

s = m:section(NamedSection, "general", translate("Gateway Properties"))
s.anonymous = true
s.addremove = false

s:tab("general",  translate("General Settings"))
s:tab("radios",  translate("Radio Settings"))
s:tab("channels", translate("Channels Settings"))
s:tab("info", translate("Info Display"))

----
---- General Settings
----

o = s:taboption("general", ListValue, "provider", translate("Service Provider"))
o.optional = true
o.default = "ttn"
o:value("ttn", "The Things Network")
o:value("custom", "--custom--")

o = s:taboption("general", Value, "custom_server", translate("LoraWAN server Address"))
o.optional = true
o.placeholder = "Domain or IP"
o.datatype    = "host"
o:depends("provider", "custom")

o = s:taboption("general", ListValue, "ttn_server", translate("Server Address"))
o:depends("provider","ttn")
o.default = "router.eu.thethings.network"
o:value("router.eu.thethings.network", "ttn-router-eu")
o:value("router.us.thethings.network", "ttn-router-us-west")
o:value("router.as.thethings.network", "ttn-router-asia-se")
o:value("router.as1.thethings.network", "ttn-router-asia-se-1")
o:value("router.as2.thethings.network", "ttn-router-asia-se-2")
o:value("router.cn.thethings.network", "router.cn.thethings.network")
o:value("router.kr.thethings.network", "router.kr.thethings.network")
o:value("router.jp.thethings.network", "router.jp.thethings.network")
o:value("thethings.meshed.com.au", "ttn-router-asia-se")
o:value("as923.thethings.meshed.com.au", "meshed-router")
o:value("ttn.opennetworkinfrastructure.org", "switch-router")

o = s:taboption("general", Value, "upp", translate("Server port for upstream"))
o.optional = true
o.placeholder = "port"
o.datatype    = "port"
o.default = "1700"

o = s:taboption("general", Value, "dpp", translate("Server port for downstream"))
o.optional = true
o.placeholder = "port"
o.datatype    = "port"
o.default = "1700"

o = s:taboption("general", Value, "GWID", translate("Gateway ID"))
o.optional = true
o.placeholder = "GatewayID"
o.datatype = "rangelength(16,16)"

o = s:taboption("general", Value, "stat", translate("Status keepalive in seconds"))
o.optional = true
o.default = "30"
o.placeholder = "seconds"
o.datatype    = "integer"

o = s:taboption("general", ListValue, "gwcfg", translate("SX1301 Configure"))
o.optional = true
o.placeholder = "SX1301 Configure"
o:value("EU", "Europe 863_870MHz")
o:value("CN", "China 470-510MHz")
o:value("US", "United_States 902_928MHz")
o:value("AU", "Australia 915_928MHz")
o:value("IN", "India 865-867MHz")
o:value("KR", "Korea 920-923MHz")
o:value("AS1", "Asia 920-923MHz")
o:value("AS2", "Asia 923-925MHz")

o = s:taboption("general", Flag, "master", translate("customer radios configure"))
o.optional = true
o.default = 0
o.disable = 0
o.enable = 1

o = s:taboption("general", Flag, "sx1276_tx", translate("use sx1276 for tx"))
o.optional = true
o.default = 0
o.disable = 0
o.enable = 1


----
---- Radio Settings
----

o = s:taboption("radios", Flag, "radio0_enable", translate("radio 0 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("radios", Value, "radio0_freq", translate("Radio_0 frequency"))
o.optional = true
o.default = 867500000
o.placeholder = "9 digits Frequency, etc:867500000"
o.datatype = "rangelength(9,9)"
o:depends("radio0_enable", "1")

o = s:taboption("radios", Flag, "radio0_tx", translate("Radio_0 for tx"))
o.optional = true
o.default = 0
o.disable = 0
o.enable = 1
o:depends("radio0_enable", "1")

o = s:taboption("radios", Value, "radio0_txfreq_min", translate("Radio_0 tx min frequency"))
o.optional = true
o.default = 863000000
o.placeholder = "9 digits Frequency, etc:868100000"
o.datatype = "rangelength(9,9)"
o:depends("radio0_tx", "1")

o = s:taboption("radios", Value, "radio0_txfreq_max", translate("Radio_0 tx max frequency"))
o.optional = true
o.default = 870000000
o.placeholder = "9 digits Frequency, etc:868100000"
o.datatype = "rangelength(9,9)"
o:depends("radio0_tx", "1")

o = s:taboption("radios", Flag, "radio1_enable", translate("radio 1 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("radios", Value, "radio1_freq", translate("Radio_1 frequency"))
o.optional = true
o.default = 868500000
o.placeholder = "9 digits Frequency, etc:867500000"
o.datatype = "rangelength(9,9)"
o:depends("radio1_enable", "1")

o = s:taboption("radios", Flag, "radio1_tx", translate("Radio_1 for tx"))
o.optional = true
o.default = 1
o.disable = 0
o.enable = 1
o:depends("radio1_enable", "1")

o = s:taboption("radios", Value, "radio1_txfreq_min", translate("Radio_1 tx min frequency"))
o.optional = true
o.placeholder = "9 digits Frequency, etc:868100000"
o.datatype = "rangelength(9,9)"
o:depends("radio1_tx", "1")

o = s:taboption("radios", Value, "radio1_txfreq_max", translate("Radio_1 tx max frequency"))
o.optional = true
o.placeholder = "9 digits Frequency, etc:868100000"
o.datatype = "rangelength(9,9)"
o:depends("radio1_tx", "1")

----
---- Channels Settings
----
o = s:taboption("channels", Flag, "chan0_enable", translate("multiSF channel 0 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan0_radio", translate("multiSF channel 0 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan0_enable", "1")

o = s:taboption("channels", Value, "chan0", translate("multiSF channel 0 IF"))
o.optional = true
o.default = -400000
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan0_enable", "1")

o = s:taboption("channels", Flag, "chan1_enable", translate("multiSF channel 1 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan1_radio", translate("multiSF channel 1 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan1_enable", "1")

o = s:taboption("channels", Value, "chan1", translate("multiSF channel 1 IF"))
o.optional = true
o.default = -200000
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan1_enable", "1")

o = s:taboption("channels", Flag, "chan2_enable", translate("multiSF channel 2 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan2_radio", translate("multiSF channel 2 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan2_enable", "1")

o = s:taboption("channels", Value, "chan2", translate("multiSF channel 2 IF"))
o.optional = true
o.default = 0
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan2_enable", "1")

o = s:taboption("channels", Flag, "chan3_enable", translate("multiSF channel 3 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan3_radio", translate("multiSF channel 3 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan3_enable", "1")

o = s:taboption("channels", Value, "chan3", translate("multiSF channel 3 IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan3_enable", "1")

o = s:taboption("channels", Flag, "chan4_enable", translate("multiSF channel 4 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan4_radio", translate("multiSF channel 4 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan4_enable", "1")

o = s:taboption("channels", Value, "chan4", translate("multiSF channel 4 IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan4_enable", "1")

o = s:taboption("channels", Flag, "chan5_enable", translate("multiSF channel 5 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan5_radio", translate("multiSF channel 5 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan5_enable", "1")

o = s:taboption("channels", Value, "chan5", translate("multiSF channel 5 IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan5_enable", "1")

o = s:taboption("channels", Flag, "chan6_enable", translate("multiSF channel 6 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan6_radio", translate("multiSF channel 6 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan6_enable", "1")

o = s:taboption("channels", Value, "chan6", translate("multiSF channel 6 IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan6_enable", "1")

o = s:taboption("channels", Flag, "chan7_enable", translate("multiSF channel 7 enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "chan7_radio", translate("multiSF channel 7 radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("chan7_enable", "1")

o = s:taboption("channels", Value, "chan7", translate("multiSF channel 7 IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("chan7_enable", "1")

o = s:taboption("channels", Flag, "lorachan_enable", translate("lorastd channel enable"))
o.default = 1
o.disable = 0
o.enable = 1

o = s:taboption("channels", ListValue, "lorachan_radio", translate("LoRa channel radio"))
o.optional = true
o.placeholder = "select radio module"
o:value("0", "radio0")
o:value("1", "radio1")
o:depends("lorachan_enable", "1")

o = s:taboption("channels", Value, "lorachan", translate("LoRa channel IF"))
o.optional = true
o.placeholder = "IF, etc:400000, -400000"
o.datatype = "integer"
o:depends("lorachan_enable", "1")

o = s:taboption("channels", Value, "lorachan_sf", translate("LoRa channel SF"))
o.optional = true
o.placeholder = "SF, 6/7/8/9/10"
o.datatype = "uinteger"
o:depends("lorachan_enable", "1")

o = s:taboption("channels", ListValue, "lorachan_bw", translate("LoRa channel BW"))
o.optional = true
o.placeholder = "select bandwidth"
o:value("125000", "125k")
o:value("250000", "250k")
o:depends("lorachan_enable", "1")

--
-- info
--

o = s:taboption("info", TextValue, "ChanIF", translate("info"))                                
o.rows = 26                                                                                       
function o.cfgvalue()                                                                             
    return nixio.fs.readfile("/etc/lora/desc") or ""                                          
end

return m
