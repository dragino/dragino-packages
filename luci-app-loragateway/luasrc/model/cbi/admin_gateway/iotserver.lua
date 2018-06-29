m = Map("iot-services", translate("Select IoT Service"), translate("Select a IoT Service"))

s = m:section(NamedSection, "general", "iot-services", translate("Select IoT Service"))
local sv = s:option(ListValue, "server_type", translate("IoT Service"))
sv.placeholder = "Select IoT service"
sv.default = "mqtt"
sv:value("lorawan",  "Lorawan/RAW packets forwarder")
sv:value("relay",  "Lorawan/RAW packets relay")
sv:value("mqtt",  "LoraRAW forward to MQTT server")
sv:value("tcpudp",  "LoraRAW forward to TCP/UDP server")

local debug = s:option(ListValue, "logdebug", translate("Debug level"))
debug.default  = "0"
debug.value("0", "LEVEL0 only error message output");
debug.value("1", "LEVEL1 little debug message output");
debug.value("2", "LEVEL2 more verbose output");

return m
