--[[
LuCI - Lua Configuration Interface

Copyright 2008 Steven Barth <steven@midlink.org>
Copyright 2008 Jo-Philipp Wich <xm@leipzig.freifunk.net>

Copyright 2014 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id: tcp_client.lua 5948 2010-03-27 14:54:06Z jow $
]]--


local dragino_utility = require('dragino.utility')
local uci = luci.model.uci.cursor()

local uart_channels = {}
uci:foreach("sensor","channels",
	function (section)
		table.insert(uart_channels,section[".name"])
	end
)


m = Map("tcp_client", translate("TCP Client"), translate("Communicate with IoT Server through a TCP Client socket"))
s = m:section(NamedSection, "general", "settings", translate("General Settings"))

local serverip = s:option(Value, "server_address", translate("Server Address"))
serverip.placeholder = translate("IP address or Host Name") 
serverip.datatype = "host"

local serverport = s:option(Value, "server_port", translate("Server Port"))
serverport.datatype = "uinteger"

local ui = s:option(Value, "update_interval", translate("Update Interval"),translate("unit:seconds. Set to 0 to disable periodically update"))
ui.placeholder = translate("how often update data to server") 
ui.default = '60'
ui.datatype = "uinteger"

local uo = s:option(Flag, "update_onchange", translate("Update on Change"),translate("Send to server when a new value arrive"))
uo.enabled  = "1"
uo.disabled = "0"
uo.default  = uo.enabled
uo.rmempty  = false


channels = m:section(TypedSection, "channels", translate("TCP/IP Uplink Channel"))
channels.anonymous = true
channels.addremove= true
channels.template = "cbi/tblsection"
channels.extedit  = luci.dispatcher.build_url("admin/gateway/tcp_channel/%s")

channels.create = function(...)
	local sid = TypedSection.create(...)
	if sid then
		luci.http.redirect(channels.extedit % sid)
		return
	end
end

local local_id = channels:option(DummyValue, "Local_Channel", translate("Data of below channels will be uploaded"))
local_id.cfgvalue = function(self, section)
	return m.uci:get("tcp_client", section, "local_id")
end

return m
