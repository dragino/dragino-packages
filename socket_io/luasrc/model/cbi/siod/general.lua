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

local siod_id = s:option(Value, "id", "SIOD ID",translate("From 0000 to 9999"))
siod_id.datatype="range(0000,9999)"

local plcrule_help='<abbr title=\"AAAA1: ID of the SIOD to be updated when PLC rule triggers\nX1: number of local or remote output [0, 1, .. 3]\nY1: Active/not active [0, 1]\nAAAA2: ID of the SIOD to be checked in the PLC rule. If AAAA2 is empty all the rest arguments should be empty too and this notifies a PLC rule removing.\nX2: number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7))\nY2: State which triggers the PLC rule Active/not active [0, 1]\nand_or:(optional) Can be “and” or “or”\nAAAA3:(optional) ID of the SIOD to be checked in the PLC rule\nX3:(optional) number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7))\nY3:(optional) State which triggers the PLC rule Active/not active [0, 1]\">PLC Controlling Rules</abbr>'
s = m:section(NamedSection, "plcrules", "siod", translate(plcrule_help))
s.anonymous = true
s.addremove = false
local plcrule = s:option(DynamicList, "rule", translate("PLC Rules"),translate("Format: AAAA1/X1/Y1/AAAA2/X2/Y2[/and_or/AAAA3/X3/Y3]"))
plcrule.datatype="siod_plc_rule"

local plcrule_timerange='<abbr title=\"Date:(optional) Exact date 20Jul2015 or day in the week index (Monday 1, Sunday 0). Date ranges like 20Jul2015-25Jul2015 or 3-5 is also supported. Optional argument. If omitted any date assumed.\nTime:(optional) Time range specification in a format. hh1:mm1:ss1- hh2:mm2:ss2. If the second time point is smaller then the first one (and the Date does not specify a range) it is assumed that the second time is a sample from the next day. Optional argument, if omitted any time withing specified Date assumed\nexample1: Daylight:    /7:00:00-23:59:59\nexample2: Night:    /0:0:0-6:59:59 \nexample3: Friday and Saturday:   5-6/\nexample4: Sunday:   0/0:0:0-23:59:59\nexample5: Arbitrary single range:   20July2015/14:00:00-14:59:59\">PLC Rule Working Time Range</abbr>'
s = m:section(NamedSection, "timerange", "siod", translate(plcrule_timerange))
s.anonymous = true
s.addremove = false
timerange = s:option(Value, "range", translate("Time Range"),translate("Format: DATE/TIME"))
timerange.default='/'
timerange.datatype="siod_time_range"

return m
