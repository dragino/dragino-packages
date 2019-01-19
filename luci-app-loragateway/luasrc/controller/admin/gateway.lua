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
    entry({"admin", "gateway"}, alias("admin", "gateway", "iotserver"), _("Service"), 60).index = true
    if f_board == 'LG02' then
	entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg02"), _("LoRaWan GateWay"), 2)
    elseif f_board == 'LG01' then
	entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg01"), _("LoRaWan GateWay"), 2)
    else
	entry({"admin", "gateway", "gateway"}, cbi("admin_gateway/lg08"), _("LoRaWan GateWay"), 2)
    end
    entry({"admin", "gateway", "mqtt"}, cbi("admin_gateway/mqtt"), _("MQTT"), 3)
    entry({"admin", "gateway", "channel"}, cbi("admin_gateway/sub/channel"), nil).leaf = true
    entry({"admin", "gateway", "tcp_client"}, cbi("admin_gateway/tcp_client"), _("TCP_Client"), 4)
    entry({"admin", "gateway", "tcp_channel"}, cbi("admin_gateway/sub/tcp_channel"), nil).leaf = true
    entry({"admin", "gateway", "customized_script"}, cbi("admin_gateway/customized_script"), _("Customized Script"), 5)
    entry({"admin", "gateway", "lgwlog"}, template("admin_status/lgwlog"), _("Logread"), 20).leaf = true
    entry({"admin", "gateway", "lgwlog_action"}, post("lgwlog_action")).leaf = true
	
end
function lgwlog_action()
    luci.http.redirect(luci.dispatcher.build_url("admin/gateway/lgwlog"))
end


