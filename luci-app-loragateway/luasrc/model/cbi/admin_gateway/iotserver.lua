local uci = luci.model.uci.cursor()

m = Map("gateway", translate("IoT Service"))

s = m:section(NamedSection, "general", "gateway")
local sv = s:option(ListValue, "server_type", translate("IoT Service"))
sv.placeholder = "Select IoT service"
sv.default = "disabled"
sv:value("disabled",  "Disabled") 
sv:value("lorawan",  "LoRaWan/RAW forwarder")        
sv:value("relay",  "LoRaWan/RAW Relay")
sv:value("mqtt",  "LoRaRAW to MQTT")
sv:value("tcpudp",  "LoRaRAW to TCP/UDP ")

local lv = s:option(ListValue, "DEB", translate("Debug Level"))
lv.placeholder = "Select debug level"
lv.default = "1"
lv:value("0",  "No debug")
lv:value("1",  "Little message output")
lv:value("2",  "More verbose output")
lv:value("3",  "Many verbose output")

return m
