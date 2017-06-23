m = Map("gpstrack", translate("GPS Track Server"), translate("Input correct username and password provide by gpstrack vendor"))

s = m:section(NamedSection, "general", "gpstrack", translate("GPS Service"))
local tt = s:option(ListValue, "gps_service", "GPS Service")
tt:value("disabled", "Disabled")
tt:value("gpswox", "GPSWOX")
tt.default = "disabled"

s = m:section(NamedSection, "gpswox", "gpstrack", translate("GPSWOX"))        

s:option(Value, "username", translate("Username"),translate("Username of gpswox.com"))

local pass = s:option(Value, "password", translate("Password"),translate("Password of gpswox.com"))
pass.password = true

local t = s:option(ListValue, "server", translate("Map Server"))
t:value("109.235.65.195", "Europe Tracking Server")
t:value("107.170.92.234", "USA Tracking Server")
t:value("128.199.127.94", "Asia Tracking Server")
t.default = "128.199.127.94"

return m
