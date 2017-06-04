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

module("luci.controller.admin.sensor", package.seeall)

function index()
	local uci = luci.model.uci.cursor()
	local string =string
	entry({"admin", "sensor"}, alias("admin", "sensor", "service"), _("Sensor"), 30).index = true
	entry({"admin", "sensor", "service"}, cbi("admin_sensor/service"), _("IoT Service"), 1)
	entry({"admin", "sensor", "poweruart"}, cbi("admin_sensor/poweruart"), _("PowerUART"), 2)
	entry({"admin", "sensor", "mcu"}, cbi("admin_sensor/mcu"), _("MicroController"), 3)
	entry({"admin", "sensor", "flashmcu"}, call("upload_sketch"), _("Flash MCU"), 4)
	entry({"admin", "sensor", "LoRaWAN"}, cbi("admin_sensor/LoRaWAN"), _("LoRaWAN"), 5)

	--entry({"admin", "sensor", "rfgateway"}, cbi("admin_sensor/rfgateway"), _("RF Radio Gateway"), 4)

	uci:foreach("iot-services","server",
	function (section)
		if section["display"] == '1' then
			entry({"admin", "sensor", "service",section[".name"]}, cbi("admin_sensor/"..section[".name"]), _(string.upper(section[".name"])), 2)
		end
	end
	)
	
end

local function rfind(s, c)
  local last = 1
  while string.find(s, c, last, true) do
    last = string.find(s, c, last, true) + 1
  end
  return last
end

function upload_sketch()
  local sketch_hex = "/tmp/sketch.hex"

  local chunk_number = 0

  local fp
  luci.http.setfilehandler(function(meta, chunk, eof)
    if not fp then
      fp = io.open(sketch_hex, "w")
    end
    if chunk then
      chunk_number = chunk_number + 1
      fp:write(chunk)
    end
    if eof then
      chunk_number = chunk_number + 1
      fp:close()
    end
  end)

  local sketch = luci.http.formvalue("sketch_hex")
  if sketch and #sketch > 0 and rfind(sketch, ".hex") > 1 then
    local merge_output = luci.util.exec("merge-sketch-with-bootloader.lua " .. sketch_hex .. " 2>&1")
    local kill_bridge_output = luci.util.exec("kill-bridge 2>&1")
    local run_avrdude_output = luci.util.exec("run-avrdude /tmp/sketch.hex '-q -q' 2>&1")

    local ctx = {
      merge_output = merge_output,
      kill_bridge_output = kill_bridge_output,
      run_avrdude_output = run_avrdude_output
    }
    luci.template.render("dragino/upload", ctx)
  else
    luci.template.render("dragino/flashmcu")
  end
end
