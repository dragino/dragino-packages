
m = Map("lorawan", translate("Select IoT Server"), translate("Select the IoT Server type to connect"))

s = m:section(NamedSection, "general", "lorawan", translate("Select IoT Server"))
local sv = s:option(ListValue, "server_type", translate("IoT Server"))
sv.placeholder = "Select IoT server"
sv.default = "MQTT"
sv:value("LoraWAN",  "LoraWAN")
sv:value("MQTT",  "MQTT Server")
sv:value("GPSWOX",  "GPSWOX Server")
sv:value("TCP/IP",  "TCP/IP Protocol")

local debug = s:option(Flag, "pfwd_debug", translate('Debugger'),translate("TTN  Packet Forwarder Debugger"))
debug.enabled  = "1"
debug.disabled = "0"
debug.default  = debug.disabled

return m
