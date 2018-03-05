
m = Map("iot-services", translate("Select IoT Server"), translate("Select the IoT Server type to connect"))

s = m:section(NamedSection, "general", "iot-services", translate("Select IoT Server"))
local sv = s:option(ListValue, "server_type", translate("IoT Server"))
sv.placeholder = "Select IoT server"
sv.default = "mqtt"
sv:value("disabled",  "Disable")
sv:value("lorawan",  "LoRaWAN")
sv:value("mqtt",  "MQTT Server")
sv:value("gpstrack",  "GPSWOX Server")
sv:value("tcp_client",  "TCP/IP Protocol")

local debug = s:option(ListValue, "debug", translate('Log Debug Info'),translate("Show Log in System Log"))
debug.default  = "0"
debug:value("0",  "Disable Debug Info")
debug:value("1",  "Level 1")
debug:value("2",  "Level 2")

return m
