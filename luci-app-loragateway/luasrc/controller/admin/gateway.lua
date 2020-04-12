--[[
LuCI - Lua Configuration Interface

Copyright 2008 Steven Barth <steven@midlink.org>
Copyright 2008-2011 Jo-Philipp Wich <xm@subsignal.org>
Copyright 2014 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id$
]]--

module("luci.controller.admin.gateway", package.seeall)

function index()
  local uci = luci.model.uci.cursor()
  local sys = require ('luci.sys')

  --read hardware board model
  local board_file = io.open("/var/iot/board", "r")
  if nil == board_file then
	  f_board = nil
  else
	  f_board = board_file:read("*l")
  end

  local string =string
  entry({"admin", "gateway", "dragino"}, call("dragino"), _("Dragino Menu"), 1)   

  entry({"admin", "gateway"}, alias("admin", "gateway", "iotserver"), _("Service"), 60).index = true


	if f_board == 'LG02' then
		entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg02"), _("LoRaWan GateWay"), 2)
	elseif f_board == 'LG01' then
		entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg01"), _("LoRaWan GateWay"), 2)
	else 
		entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg08"), _("LoRaWan GateWay"), 2)			
	end

  entry({"admin", "gateway", "lgwlog"}, template("admin_status/lgwlog"), _("Logread"), 20).leaf = true
  entry({"admin", "gateway", "lgwlog_action"}, post("lgwlog_action")).leaf = true
end

function lgwlog_action()
    luci.http.redirect(luci.dispatcher.build_url("admin/gateway/lgwlog"))
end

------------------------------------
function mqtt() 
    luci.template.render("dragino/mqtt")    
end

function dragino() 
    luci.template.render("dragino/dragino")    
end

-- ----------------------------------

