--[[
LuCI - Lua Configuration Interface


Copyright 2015 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id$
]]--

module("luci.controller.admin.siod", package.seeall)

function index()
	entry({"admin", "socket_io"}, alias("admin", "socket_io","general"), "SIOD", 50).index = true
	entry({"admin", "socket_io","general"}, cbi("siod/general"), "General Settings", 10)
	entry({"admin", "socket_io","network"}, cbi("siod/network"), "Network", 20)
end