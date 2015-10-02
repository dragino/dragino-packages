--[[
    Copyright (C) 2014 Dragino Technology Co., Limited

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
]]--

module("luci.controller.arduino.sensor", package.seeall)

local util = require("luci.util")

function index()
	entry({ "webpanel" , "sensor"}, call("config") ,nil)
	entry({ "webpanel", "upload_sketch" }, call("upload_sketch"), nil)
end

local function not_nil_or_empty(value)
  return value and value ~= ""
end

local function config_get()
  local uci = luci.model.uci.cursor()
  uci:load("sensor")

  local board_type = {}
  board_type[1] = { code = "leonardo", label = "Leonardo, M32, M32W" }
  board_type[2] = { code = "uno", label = "Arduino Uno w/ATmega328P" }
  board_type[3] = { code = "duemilanove328", label = "Arduino Duemilanove or Diecimila w/ATmega328" }
  board_type[4] = { code = "duemilanove168", label = "Arduino Duemilanove or Diecimila w/ATmega168,MRFM12B" }
  board_type[5] = { code = "mega2560", label = "Arduino Mega2560" }
  board_type[6] = { code = "undefined", label = "Undefined" }

  local ctx = {
	board = uci:get("sensor","mcu","board"),
	board_type = board_type,
	avr_part = uci:get("sensor","mcu","avr_part"),
	upload_bootloader = uci:get("sensor","mcu","upload_bootloader"),
  }
  luci.template.render("dragino/sensor", ctx)
end

local function config_post()
  local uci = luci.model.uci.cursor()
  uci:load("sensor")

  if luci.http.formvalue("board") then
    uci:set("sensor", "mcu", "board", luci.http.formvalue("board"))
  end

  local upload_bootloader = luci.http.formvalue("upload_bootloader")
  if upload_bootloader == "enable" then
    uci:set("sensor", "mcu", "upload_bootloader", "enable")
  else 
    uci:set("sensor", "mcu", "upload_bootloader", "disable")
  end 

  uci:commit("sensor")
  luci.util.exec("/usr/bin/reset-mcu")
  config_get()
end

function config()
  if luci.http.getenv("REQUEST_METHOD") == "POST" then
    config_post()
  else
    config_get()
  end
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
    luci.template.render("arduino/upload", ctx)
  else
    luci.http.redirect(luci.dispatcher.build_url("webpanel/sensor"))
  end
end
