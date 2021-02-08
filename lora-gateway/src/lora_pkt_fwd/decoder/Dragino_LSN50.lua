#!/usr/bin/lua

json = require('dragino.json')
local utility = require("dragino.utility")
local uci = require("dragino.luci_uci")
local bit=require("dragino.bit")

local f = assert(io.open("/var/iot/channels/" .. arg[1], "rb"))

local payload = f:read("*all")

f:close()

data=""
local payload_t={}

--Battery info
HEX_BAT=string.sub(payload,17,18)
BAT_RAW=utility.hex2str(HEX_BAT)
BAT=bit.bit_and(tonumber(BAT_RAW,16),tonumber("3FFF",16))/1000  -- Got the BAT by BAT & 3FFF
payload_t.BatV=BAT

--ADC
HEX_ADC=string.sub(payload,21,22)
ADC_RAW=utility.hex2str(HEX_ADC)
payload_t.ADC=tonumber(ADC_RAW,16)

--Digitial Input
HEX_DigI=string.sub(payload,23,23)
DigI_RAW=utility.hex2str(HEX_DigI)
payload_t.DigI=tonumber(DigI_RAW,8)

--ds18b20
HEX_Tsens=string.sub(payload,19,20)
Tsens_RAW=utility.hex2str(HEX_Tsens)
payload_t.Tsens=tonumber(Tsens_RAW,16)/10

--SHT20 or SHT31
--temp
HEX_Tsht=string.sub(payload,24,25)
Tsht_RAW=utility.hex2str(HEX_Tsht)
payload_t.Tsht=tonumber(Tsht_RAW,16)/10

--humidity
HEX_Hsht=string.sub(payload,26,27)
Hsht_RAW=utility.hex2str(HEX_Hsht)
payload_t.Hsht=tonumber(Hsht_RAW,16)/10

--Server Type
local server_type = uci:get("gateway","general","server_type")
local mqtt_broker = uci:get("mqtt","common","server_type")

--[[Debug ThingSpeak Server
local server_type = "mqtt"
local mqtt_broker = "ThingSpeak"]]

if server_type == "mqtt" then
        if mqtt_broker == "ThingSpeak" then
                local data_up="&field1=".. payload_t.BatV .. "&field2=".. payload_t.ADC .. "&field3=".. payload_t.DigI .. "&field4=".. payload_t.Tsens .. "&field5=".. payload_t.Tsht .. "&field6=".. payload_t.Hsht .."&status=MQTTPUBLISH"
                print(data_up)
                return data_up
        end
end

print(json.encode(payload_t))
return json.encode(payload_t)