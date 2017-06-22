#!/usr/bin/lua

require("base64")
require("uci") 

if (arg[1] == nil) then
    return print(string.format("Usage: lua %s options packetsize freq", arg[0]))
end

local x = uci.cursor() 
local ip = x:get("LoRaWAN", "general", "server")
local port = x:get("LoRaWAN", "general", "port")
local gatewayID = x:get("LoRaWAN", "general", "gateway_id")

--[[
local ip = "192.168.204.132"
local port = "5560"
local gatewayID = "a00c29189889"
--]]
--
--  Format :
local head = string.format("%c", 1)

math.randomseed(tostring(os.time()):reverse():sub(1, 7))

local d = string.format("%c", math.random(1,255))

head = head .. d

d = string.format("%c", math.random(1,255))

head = head .. d

local len = string.len(gatewayID)

for i = 1, len, 2 do
    local s = string.sub(gatewayID, i, i + 1)
    local ascii = string.format("%c", tonumber("0x"..s))
    head = head .. ascii
end

-- stat :
if (arg[1] == "stat") then
    stat = {}
    stat[1] = "{\"stat\":{" 
    stat[2] =  "\"time\":" .. "\"" .. os.date("\%Y-\%m-\%d \%H:\%M:\%S GMT", os.time()) .."\[0:24\]\"," 
    stat[3] =  "\"lati\":139.6315," 
    stat[4] =  "\"long\":35.479," 
    stat[5] =  "\"alti\":0," 
    stat[6] =  "\"rxnb\":0," 
    stat[7] =  "\"rxok\":0," 
    stat[8] =  "\"rxfw\":0," 
    stat[9] =  "\"ackr\":0," 
    stat[10] =  "\"dwnb\":0," 
    stat[11] =  "\"txnb\":0," 
    stat[12] =  "\"pfrm\":\"Dragino LG01-JP\"," 
    stat[13] =  "\"mail\":\"asuzuki@openwave.co.jp\"," 
    stat[14] =  "\"desc\":\"\"" 
    stat[15] =  "}}" 

    head = head .. table.concat(stat)

    stat = nil

else
    rxpk = {}
    rxpk[1] =   "{\"rxpk\":[{" 
    rxpk[2] =   "\"tmst\":\"" .. os.time() .. "\[0:10\]\"," 
    rxpk[3] =   "\"chan\":0," 
    rxpk[4] =   "\"rfch\":0," 
    rxpk[5] =   "\"freq\":\"" .. arg[3] .. "\"," 
    rxpk[6] =   "\"stat\":1," 
    rxpk[7] =   "\"modu\":\"LORA\"," 
    rxpk[8] =   "\"datr\":\"SF7" 
    rxpk[9] =   "BW125\","
    rxpk[10] =  "\"codr\":\"4/5\"," 
    rxpk[11] =  "\"lsnr\":9," 
    rxpk[12] =  "\"rssi\":\"" .. arg[1] .. "\"," 
    rxpk[13] =  "\"size\":\"" .. arg[2] .. "\"," 
    rxpk[14] =  "\"data\":\""
    
    f = io.open('/root/data/bin','rb')
    data1 = f:read("*a")
    f:close()

    head = head .. table.concat(rxpk) .. base64.b64encode(data1) .. "\"}]}"

    rxpk = nil

end

-- print test:
print(head)


-- sendto UDP Server
socket = require "nixio".socket 

sock = socket("inet", "dgram")
sock:sendto(head, ip, port)
sock:close()

