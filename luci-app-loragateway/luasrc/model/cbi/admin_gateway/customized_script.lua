--[[
LuCI - Lua Configuration Interface

Copyright 2019 Dragino Tech <support@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

]]--


local fs = require "nixio.fs"
local uci = luci.model.uci.cursor()

local scripts = {}
uci:foreach("sensor","channels",
	function (section)
		table.insert(uart_channels,section[".name"])
	end
)


m = Map("customized_script", translate("Customized Script"), translate("Run a Customized Script to process LoRa Data, parameters are optional and defined in script"))
s = m:section(NamedSection, "general", "settings", translate("General Settings"))

local sv = s:option(ListValue, "script_name", translate("Script Name"))
sv.placeholder = "Script Name"
for file in fs.dir("/etc/lora/customized_scripts") do
	sv:value(file,file)
end

s:option(Value, "para1", translate("Parameter 1"))
s:option(Value, "para2", translate("Parameter 2"))
s:option(Value, "para3", translate("Parameter 3"))
s:option(Value, "para4", translate("Parameter 4"))
s:option(Value, "para5", translate("Parameter 5"))
s:option(Value, "para6", translate("Parameter 6"))
s:option(Value, "para7", translate("Parameter 7"))
s:option(Value, "para8", translate("Parameter 8"))
s:option(Value, "para9", translate("Parameter 9"))
s:option(Value, "para10", translate("Parameter 10"))


return m
