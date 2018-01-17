
m = Map("iot-services", translate("Select IoT Server"), translate("Select the IoT Server type to connect"))

s = m:section(NamedSection, "general", "iot-services", translate("Select IoT Server"))
local sv = s:option(ListValue, "server_type", translate("IoT Server"))
sv.placeholder = "Select IoT server"
sv.default = "mqtt"
sv:value("disabled",  "Disable")
sv:value("LoraWAN",  "LoRaWAN")
sv:value("mqtt",  "MQTT Server")
sv:value("GPSWOX",  "GPSWOX Server")
sv:value("TCP/IP",  "TCP/IP Protocol")

local debug = s:option(Flag, "debug", translate('Enable Log Info'),translate("Show Log in System Log"))
debug.enabled  = "1"
debug.disabled = "0"
debug.default  = debug.disabled

return m
