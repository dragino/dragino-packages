--[[
LuCI - Lua Configuration Interface

Copyright 2015 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0 

]]--


m = Map("network", translate("Network"))
s = m:section(NamedSection, "bat", "network", translate("Network"))
s.addremove = false

local p = s:option(ListValue, "proto", "Way to Get IP")
p:value("static", "Static IP")
p:value("dhcp", "DHCP")

local ip = s:option(Value, "ipaddr", "IP Address")
ip:depends("proto", "static")
ip.datatype = "ipaddr"

local nm = s:option(Value, "netmask", "Netmask")
nm.default = "255.255.255.0"
nm:depends("proto", "static")
nm:value("255.255.255.0")
nm:value("255.255.0.0")
nm:value("255.0.0.0")
nm.datatype = "ipaddr"

local gw = s:option(Value, "gateway", "Gateway")
gw:depends("proto", "static")
gw.rmempty = true
gw.datatype = "ipaddr"

local dns = s:option(Value, "dns", "DNS Server")
dns:depends("proto", "static")
dns.rmempty = true
dns.datatype = "host"
dns.placeholder = "DNS server domain or IP"

m1 = Map("wireless", translate("Mesh"))
s = m1:section(NamedSection, "ah_0", "wireless", translate("Mesh"))
s.addremove = false

local me = s:option(Flag, "disabled", translate("Enable Mesh"),"Enable Mesh Network")
me.enabled  = "0"
me.disabled = "1"
me.default  = me.disabled
me.rmempty  = false

s:option(Value, "ssid", "SSID")
s:option(Value, "bssid", "BSSID")

local encry = s:option(ListValue, "encryption", "Encryption")
encry:value("none","None")
encry:value("wep","WEP")
encry:value("psk","WPA")
encry:value("psk2","WPA2")
encry:value("mixed-psk","WPA-WPA2")


local pwd = s:option(Value, "key", "Passphrase")
pwd.password = true
pwd:depends("encryption","psk")
pwd:depends("encryption","psk2")
pwd:depends("encryption","wep")

return m,m1