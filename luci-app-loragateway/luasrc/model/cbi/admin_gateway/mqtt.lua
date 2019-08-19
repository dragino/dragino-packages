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
local server_t

m = Map("mqtt", translate("MQTT Server Settings"), translate("Configuration to communicate with MQTT server"))

s = m:section(NamedSection, "general", "mqtt", translate("Configure MQTT Server"))

local st = s:option(ListValue, "server_type", translate("Select Server"))
st.placeholder = "Select MQTT Server"
st.default = "general"
st:value("general",  "General Server")
st:value("lewei50",  "Lewei50")
st:value("thingspeak",  "ThingSpeak MQTT")
st:value("mydevices",  "myDevices")
st:value("azureiothub",  "Azure IoTHub")
--function st.write(self,section,value)
--	server_t = value
--	m.uci:set("mqtt","general","server_type",value)
--	m.uci:set("mqtt","general","server","mqtt.thingspeak.com")
--	if (value == "thingspeak") then
--		m.uci:set("mqtt","general","server","mqtt.thingspeak.com")
--	end
--end

local sv = s:option(Value, "server", translate("Broker Address [-h]"))
sv.datatype = "host"
sv.placeholder = "Domain or IP"
sv:depends("server_type","general")
sv:depends("server_type","azureiothub")
--sv.rmempty  = false

local sp = s:option(Value, "port", translate("Broker Port [-p]"))
sp.datatype = "port"
sp.default = "1883"
sp:depends("server_type","general")
--sp.rmempty  = false

local user = s:option(Value, "username", translate("User Name [-u]"))
user.placeholder = "MQTT User Name"

local password = s:option(Value, "password", translate("Password [-P]"))
password.placeholder = "MQTT password"

local cid = s:option(Value, "client_id", translate("Client ID [-i]"))
cid.placeholder = "MQTT Client ID"

local qos = s:option(ListValue, "QoS", translate("Quality of service level [-q]"))
qos.default = "0"
qos:value("0",  "QoS 0")
qos:value("1",  "QoS 1")
qos:value("2",  "QoS 2")

--local api_key = s:option(Value, "api_key", translate("API Key"))
--api_key.placeholder = "MQTT API Key"
--api_key:depends("server_type","general")

local topic_format = s:option(Value, "topic_format", translate("Topic Format [-t]"))
topic_format.placeholder = "MQTT publish topic format"
topic_format:depends("server_type","general")

local data_format = s:option(Value, "data_format", translate("Data String Format [-m]"))
data_format.placeholder = "MQTT publish data format"
data_format:depends("server_type","general")

--local en_sub = s:option(Flag, "en_sub", translate("Enabled Subscribe"))
--en_sub.enabled  = "1"
--en_sub.disabled = "0"
--en_sub.default  = en_sub.disable
--en_sub.rmempty  = false

--local sub_topic = s:option(Value, "sub_topic", translate("Subscribe Topic"))
--sub_topic.placeholder = "MQTT subscribe topic"

--local sub_action = s:option(Value, "action", translate("Downlink Action"),translate("Action when there subscribed topic update"))

channels = m:section(TypedSection, "channels", translate("MQTT Channel"),translate("Match between Local Channel and remote channel"))
channels.anonymous = true
channels.addremove=true
channels.template = "cbi/tblsection"
channels.extedit  = luci.dispatcher.build_url("admin/gateway/channel/%s")

channels.create = function(...)
	local sid = TypedSection.create(...)
	if sid then
		luci.http.redirect(channels.extedit % sid)
		return
	end
end

local local_id = channels:option(DummyValue, "Local_Channel", translate("Local Channel in /var/iot/channels/"))
local_id.cfgvalue = function(self, section)
	return m.uci:get("mqtt", section, "local_id")
end

local remote_id = channels:option(DummyValue, "Remote_Channel", translate("Remote Channel in IoT Server"))
remote_id.cfgvalue = function(self, section)
	return m.uci:get("mqtt", section, "remote_id")
end

local write_api_key = channels:option(DummyValue, "Write_API_Key", translate("Write API Key"))
write_api_key.cfgvalue = function(self, section)
	return m.uci:get("mqtt", section, "write_api_key")
end

return m
