--[[
LuCI - Lua Configuration Interface

Copyright 2017 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id: mqtt.lua 5948 2010-03-27 14:54:06Z jow $
]]--

local uci = luci.model.uci.cursor()

m = Map("mqtt", translate("MQTT Server Settings"), translate("Configuration to communicate with MQTT server"))

s = m:section(NamedSection, "general", "lorawan", translate("MQTT Server Settings"))
local sv = s:option(Value, "server", translate("Server Address"))
sv.datatype = "host"
sv.placeholder = "Domain or IP"

local sp = s:option(Value, "port", translate("Server Port"))
sp.datatype = "port"
sp.default = "1883"


local user = s:option(Value, "user_name", translate("User Name"))
user.placeholder = "MQTT User Name"


local password = s:option(Value, "password", translate("Password"))
password.placeholder = "MQTT password"

local cid = s:option(Value, "client_id", translate("Client ID"))
cid.placeholder = "MQTT Client ID"

local pub_topic = s:option(Value, "pub_topic", translate("Publish Topic"))
pub_topic.placeholder = "MQTT publish topic"

local en_sub = s:option(Flag, "en_sub", translate("Enabled Subscribe"))
en_sub.enabled  = "1"
en_sub.disabled = "0"
en_sub.default  = en_sub.disable
en_sub.rmempty  = false

local sub_topic = s:option(Value, "sub_topic", translate("Subscribe Topic"))
sub_topic.placeholder = "MQTT subscribe topic"

local sub_action = s:option(Value, "action", translate("Downlink Action"),translate("Action when there subscribed topic update"))



return m