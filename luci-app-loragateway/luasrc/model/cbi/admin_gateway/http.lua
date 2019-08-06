--[[
LuCI - Lua Configuration Interface

Copyright 2019 Dragino Tech <support@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

]]--


--local fs = require "nixio.fs"
local uci = luci.model.uci.cursor()


m = Map("http_iot", translate("HTTP / HTTPS"), translate("http/https connection to IoT Server"))
s = m:section(NamedSection, "general", "settings", translate("General Settings"))

--local st = s:option(ListValue, "server_type", translate("Select Server"))
--st.placeholder = "Select HTTP Server"
--st.default = "general"
--st:value("general",  "General Server")
--st:value("thingspeak",  "ThingSpeak")

local ssl=s:option(Flag, "SSL", translate("Enable SSL Connection"))
ssl.enabled  = "1"
ssl.disabled = "0"
ssl.default  = ssl.disable
ssl.rmempty  = false

--local up=s:option(Flag, "up_enable", translate("Enable HTTP Uplink"))
--up.enabled  = "1"
--up.disabled = "0"
--up.default  = up.disable
--up.rmempty  = false

local down=s:option(Flag, "down_enable", translate("Enable HTTP Downlink"),translate("Forward downlink data via LoRa"))
down.enabled  = "1"
down.disabled = "0"
down.default  = down.disable
down.rmempty  = false


local du=s:option(Value, "down_url", translate("Downlink URL"))

local dd = s:option(ListValue, "down_type", translate("Downlink Datatype"))
dd.default = "json"
dd:value("json",  "Json - One Level")

local dp = s:option(Value, "down_para", translate("Downlink Parameter"))

local dt = s:option(Value, "down_interval", translate("Downlink Poll Interval"),translate("unit:seconds."))
dt.datatype = "integer"
dt.default = "10"

return m
