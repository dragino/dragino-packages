
m = Map("sensor", translate("Select IoT Server"), translate("Select IoT Server"))
local sv = s:option(ListValue, "server", translate("IoT Server"))
sv.placeholder = "Select IoT server"
sv.default = "MQTT"
sv:value("LoraWAN",  "LoraWAN")
sv:value("MQTT",  "MQTT Server")
sv:value("GPSWOX",  "GPSWOX Server")
sv:value("TCP/IP",  "TCP/IP Protocol")

return m
