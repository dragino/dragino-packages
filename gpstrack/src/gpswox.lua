local sys = require "luci.sys"
require("uci")

m = Map("gpstrack", translate("Gpstrack settings"), translate("input correct username and password provide by gpstrack vendor"))
s = m:section(NamedSection, "gpswox", "gpstrack", translate("GPSWOX"))        
local deviceid = s:option(DummyValue, "deviceID", translate("deviceID"),translate("Device Identify, generat by Dragino CO."))
function deviceid.cfgvalue(self,section)
    local r = sys.exec('ifconfig eth0 | grep "HWaddr"') or "no deviceid!"
    r = string.sub(r, -20)
    r = string.gsub(r, ":", "1")
    r = string.sub(r, 4, 11)    
    r = tonumber(r, 16)
    x = uci.cursor()            
    x:set("gpstrack", "gpswox", "deviceID", r)                                        
    x:commit("gpstrack")
    return r
end
s:option(Value, "username", translate("Username"),translate("Username of gpswox.com"))
s:option(Value, "password", translate("Password"),translate("Password of gpswox.com"))
s:option(Value, "frequency", translate("Frequency"),translate("How many seconds"))                                    

local t = s:option(ListValue, "server", translate("Map Server"))
    t:value("1", "Europe Tracking Server")
    t:value("2", "USA Tracking Server")
    t:value("3", "Asia Tracking Server")
    t.default = "2"

local ss = s:option(Flag, "status", translate("Service status"),translate("enable or disable service of gpswox"))
    ss.enabled  = "enable"                                                                                              
    ss.disabled = "disable"
    ss.default  = ss.disabled
    ss.rmempty  = false

return m
