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

m = Map("gateway", translate("LoRa Gateway Settings"), translate("Configuration to communicate with LoRa devices and LoRaWAN server"))

s = m:section(NamedSection, "general", "gateway", translate("LoRaWAN Server Settings"))

local sv = s:option(Value, "server", translate("Server Address"))
sv.datatype = "host"
sv.placeholder = "Domain or IP"

local upp = s:option(Value, "upp", translate("Server Port UPstream"))
upp.datatype = "port"
upp.default = "1700"

local dwp = s:option(Value, "dwp", translate("Server Port DWstream"))
dwp.datatype = "port"
dwp.default = "1700"


local gid = s:option(Value, "gwid", translate("Gateway ID"))
gid.placeholder = "Gateway ID"


local mail = s:option(Value, "mail", translate("Mail Address"))
mail.placeholder = "Mail address sent to Server"

local lati = s:option(Value, "lati", translate("Latitude"))
lati.placeholder = "Location Info"

local long = s:option(Value, "long", translate("Longtitude"))
long.placeholder = "Location Info"

local gwcfg = s:option(ListValue, "gwcfg", translate("Gateway Configure"))
gwcfg.placeholder = "Gateway Configure"
gwcfg.default = "EU868.basic"
gwcfg:value("EU868.basic", "EU868-basic")
gwcfg:value("EU868.beacon", "EU868-beacon")
gwcfg:value("EU868.gps", "EU868-gps")
gwcfg:value("US902.basic", "US902-basic")
gwcfg:value("US902.beacon", "US902-beacon")
gwcfg:value("US902.gps", "US902-gps")

return m
