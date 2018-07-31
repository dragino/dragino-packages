local uci = luci.model.uci.cursor()

m = Map("iot-services", translate("IoT Service"))

s = m:section(NamedSection, "general", "iot-services")
local sv = s:option(ListValue, "server_type", translate("IoT Service"))
sv.placeholder = "Select IoT service"
sv.default = "lorawan"
sv:value("lorawan",  "Lorawan/RAW forwarder")        
sv:value("relay",  "Lorawan/RAW packets relay")
sv:value("mqtt",  "LoraRAW forward to MQTT server")
sv:value("tcpudp",  "LoraRAW forward to TCP/UDP server")

local lv = s:option(ListValue, "logdebug", translate("Debug Level"))
lv.placeholder = "Select debug level"
lv.default = "0"
lv:value("0",  "No debug")
lv:value("1",  "Little message output")
lv:value("2",  "More verbose output")

return m
