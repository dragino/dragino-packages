--[[
LuCI - Lua Configuration Interface

Copyright 2015 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0 

]]--


m = Map("siod", translate("Socket IO"))
s = m:section(NamedSection, "siod_id", "siod", translate("SIOD ID"))
s.addremove = false

local siod_id = s:option(Value, "id", "SIOD ID")
siod_id.datatype="uinteger"

return m