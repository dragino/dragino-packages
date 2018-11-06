#!/usr/bin/lua

--[[
tcp_client.lua

Send data to remote TCP server

Copyright 2014 Dragino Tech <info@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

]]--

sensor_dir='/var/iot/channels'
local socket = require('socket')
local uci = require("luci.model.uci")
uci = uci.cursor()
local utility = require 'dragino.utility'
local luci_util = require("luci.util")
local json = require("luci.json")


local server = uci:get("tcp_client","general","server_address")
local port = uci:get("tcp_client","general","server_port")
local interval = uci:get("tcp_client","general","update_interval")
local debug = tonumber(uci:get("gateway","general","DEB"))


local luci_fs = require("nixio.fs")

local sensor_id_table={}

--Polish tcp_client channel config
--And get sensor channels id
uci:foreach("tcp_client","channels",
	function (section)
		if section.local_id ~= nil then
			sensor_id_table[section.local_id]=section.local_id
			luci_util.exec("uci rename tcp_client."..section[".name"].."="..section.local_id)
		else 
			uci:delete("tcp_client",section[".name"])
		end  
	end
)
uci:save("tcp_client")

local old_time = os.time()
local cur_time = old_time


while 1
do 
	cur_time = os.time()
	if cur_time - old_time >= tonumber(interval) then
		--Get All Sensor Data
		local sensor_value_table={}
		sensor_value_table=utility.get_channels_valuetable()
		local tdata={}
		local jdata=nil
		
		--Add wanted data
		count=0
		for id,value in pairs(sensor_value_table) do 
			if sensor_id_table[id] ~= nil then
				tdata[id]=value
				count=count+1
			end
		end
		--utility.tabledump(tdata)
		if count > 0 then
			jdata = json.encode(tdata)
			if debug >= 1 then luci_util.exec("logger [IoT:]TCP_IP Client: " .. jdata) end
			luci_util.exec("rm -rf " .. sensor_dir .. "/*")
			local client = socket.connect(server, port)

			client:send(jdata)
			client:close()
		end
		old_time = cur_time
	end 
end 
