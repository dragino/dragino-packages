#!/usr/bin/lua

require("uci") 
local utility = require "dragino.utility"

if (arg[1] == nil) then
    return print(string.format("Usage: lua %s stat packetsize freq", arg[0]))
end

local x = uci.cursor() 
local ip = x:get("lorawan", "general", "server")
local port = x:get("lorawan", "general", "port")
local gatewayID = x:get("lorawan", "general", "gateway_id")
local email = x:get("lorawan", "general", "mail")
local lati = x:get("lorawan", "general", "lati")
local long = x:get("lorawan", "general", "long")
local SF = x:get("lorawan", "radio", "SF")
local frequency = x:get("lorawan", "radio", "frequency")
local coderate = x:get("lorawan", "radio", "coderate")
local BW = x:get("lorawan", "radio", "BW")

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

d = string.format("%c", 0)

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
    stat[2] =  "\"time\":" .. "\"" .. os.date("\%Y-\%m-\%d \%H:\%M:\%S GMT", os.time()) .."\"," 
    stat[3] =  "\"lati\":" .. lati .."," 
    stat[4] =  "\"long\":" .. long .."," 
    stat[5] =  "\"alti\":0," 
    stat[6] =  "\"rxnb\":0," 
    stat[7] =  "\"rxok\":0," 
    stat[8] =  "\"rxfw\":0," 
    stat[9] =  "\"ackr\":0," 
    stat[10] =  "\"dwnb\":0," 
    stat[11] =  "\"txnb\":0," 
    stat[12] =  "\"pfrm\":\" Dragino LG01/OLG01\"," 
    stat[13] =  "\"mail\":\"".. email .."\"," 
    stat[14] =  "\"desc\":\"\"" 
    stat[15] =  "}}" 

    print(table.concat(stat))

    head = head .. table.concat(stat)

    stat = nil

else
    rxpk = {}
    rxpk[1] =   "{\"rxpk\":[{" 
    rxpk[2] =   "\"tmst\":\"" .. os.time() .. "\[0:10\]\"," 
    rxpk[3] =   "\"chan\":0," 
    rxpk[4] =   "\"rfch\":0," 
    rxpk[5] =   "\"freq\":\"" .. frequency .. "\"," 
    rxpk[6] =   "\"stat\":1," 
    rxpk[7] =   "\"modu\":\"LORA\"," 
    rxpk[8] =   "\"datr\":\"" .. SF
    rxpk[9] =   BW .."\","
    rxpk[10] =  "\"codr\":\"".. coderate .."\"," 
    rxpk[11] =  "\"lsnr\":9," 
    rxpk[12] =  "\"rssi\":\"" .. arg[1] .. "\"," 
    rxpk[13] =  "\"size\":\"" .. arg[2] .. "\"," 
    rxpk[14] =  "\"data\":\""
    
    f = io.open('/var/iot/data','rb')
    if f then
        data1 = f:read("*a")
        f:close()
        head = head .. table.concat(rxpk) .. utility.b64encode(data1) .. "\"}]}"
    end

    rxpk = nil

end

-- sendto UDP Server
socket = require "nixio".socket 

sock = socket("inet", "dgram")
sock:sendto(head, ip, port)
sock:close()

