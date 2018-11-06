--[[
LuCI - Lua Configuration Interface
Copyright 2015 Edwin Chen <edwin@dragino.com>
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
	http://www.apache.org/licenses/LICENSE-2.0
$Id: client.lua 5948 2015-02-03 Dragino Tech $
]]--

local uci = luci.model.uci.cursor()
local tonumber=tonumber

m = Map("tcp_client", translate("Sensor Channels"))
m.redirect = luci.dispatcher.build_url("admin/gateway/tcp_client")

if not arg[1] or m.uci:get("tcp_client", arg[1]) ~= "channels" then
	luci.http.redirect(m.redirect)
	return
end


cc = m:section(NamedSection, arg[1], "channels", translate("Data of below channels will be uploaded"))
cc.anonymous = true
cc.addremove = false

local local_id = cc:option(Value, "local_id", translate("Channel ID"))

return m
