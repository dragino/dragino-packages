--[[
LuCI - Lua Configuration Interface

Copyright 2008 Steven Barth <steven@midlink.org>
Copyright 2008 Jo-Philipp Wich <xm@leipzig.freifunk.net>

Copyright 2013 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id: mcu.lua 5948 2010-03-27 14:54:06Z jow $
]]--

local uci = luci.model.uci.cursor()

local Auto_update_helper='Device will download update info from [Update_Url]/[Update Info File]\n'..
				'Update Info File Format: \n'..
				'	image=IMAGE_NAME\n' ..
				'	md5sum=md5\n'..
				'	version=version\n\n'..
				'If version > current version, device will download the [Update_Url]/image\n'..
				'and [Update_Url]/md5sum to update the MCU\n'..
				'Device will update current version to version if update successful\n\n'..
				'If mac_identify is set, device will download update info from [Update_Url]/[wifi_mac.txt] \n instead of [Update_Url]/[Update Info File]'
				
local Update_Info='File Format:\n' ..
				'   image=IMAGE_NAME\n' ..
				'   md5sum=md5\n'..
				'   version=version\n'

m = Map("sensor", translate("Micro-Controller settings"), translate("Configure correct Arduino profile will let you to able upload avr program via Arduino IDE and WiFi"))

s = m:section(NamedSection, "mcu", "sensor", translate("MCU Upload Profile"))
s:option(Value, "avr_part", translate("AVR Part"),translate("Auto detected by software on boot"))

local board = s:option(ListValue, "board", translate("Profile"),translate("Auto detected by software on boot"))
board:value('leonardo','Leonardo, M32, M32W')
board:value('uno','Arduino Uno w/ATmega328P')
board:value('duemilanove328','Arduino Duemilanove or Diecimila w/ATmega328')
board:value('duemilanove168','Arduino Duemilanove or Diecimila w/ATmega168,MRFM12B')
board:value('mega2560','Arduino Mega2560')
board:value('undefined','Undefined')

local uo = s:option(Flag, "upload_bootloader", translate("Add Bootloader"),translate("Add Arduino bootloader while upload"))
uo.enabled  = "enable"
uo.disabled = "disable"
uo.default  = uo.disabled
uo.rmempty  = false

s = m:section(NamedSection, "auto_update", "sensor", translate('<abbr title=\"'..Auto_update_helper..'\">Auto Update MCU Image</abbr>'))
local ab = s:option(Flag, "update_on_boot", translate('Auto Update On Boot'),translate("Auto update once device boot"))
ab.enabled  = "1"
ab.disabled = "0"
ab.default  = ab.disabled
ab.rmempty  = false

--local ap = s:option(Flag, "update_periodic", translate('Update Period'),translate("Auto update at certain time"))
--ap.enabled  = "1"
--ap.disabled = "0"
--ap.default  = ap.disabled
--ap.rmempty  = false

--local ud = s:option(Value, "day", translate('Day'),translate("Day of a month (1~30)"))
--ud:depends("update_periodic","1")
--ud.default  = "1"

--local uh = s:option(Value, "hour", translate('Day'),translate("Hour of a day (1~24)"))
--uh:depends("update_periodic","1")
--uh.default  = "1"


s:option(Value, "current_ver", translate("Current Image Version"),translate("Current Image Version used in the MCU"))

s:option(Value, "update_url", translate("Update URL"),translate("Get Update Info from this URL"))

local mi = s:option(Flag, "mac_identify", translate('<abbr title="Get update info from wifi-mac.txt (etc: A84041123456.txt)">Enable MAC Identify</abbr>'),translate("maintain different update info for different device"))
mi.enabled  = "1"
mi.disabled = "0"
mi.default  = mi.disabled
mi.rmempty  = false

local ui = s:option(Value, "update_info", translate('<abbr title=\"'..Update_Info..'\">Update Info</abbr>'),translate("File Includes Update Information"))
ui.placeholder = translate("autoupdate.txt")


return m