"use strict";

/*
 *   Copyright (C) 2014 Dragino Technology Co., Limited (http://www.dragino.com/)
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 */

function onchange_uartmode(select) {
  var uartbaud_container = document.getElementById("uartbaud_container");
  if (select.value == "bridge") {
    uartbaud_container.setAttribute("class", "hidden");
  } else {
    uartbaud_container.removeAttribute("class");
  }
}

function onchange_server(select) {
  var tenant_container = document.getElementById("tenant_container");
  var user_container = document.getElementById("user_container");
  var pass_container = document.getElementById("pass_container");
  var globalid_container = document.getElementById("globalid_container");
  var deviceid_container = document.getElementById("deviceid_container");
  if (select.value != "cumulocity") {
    tenant_container.setAttribute("class", "hidden");
    user_container.setAttribute("class", "hidden");
    pass_container.setAttribute("class", "hidden");
    globalid_container.setAttribute("class", "hidden");
  } else {
    tenant_container.removeAttribute("class");
    user_container.removeAttribute("class");
    pass_container.removeAttribute("class");
    globalid_container.removeAttribute("class");
  }
  if (select.value != "xively" && select.value != "yeelink") {
    deviceid_container.setAttribute("class", "hidden");
  } else {
    deviceid_container.removeAttribute("class");
	if (select.value == "xively")  document.getElementById("deviceid_label").innerHTML="Feed ID";
	else document.getElementById("deviceid_label").innerHTML="Device ID";
  }
}

document.body.onload = function() {
  var server = document.getElementById("server");
  if (server) {
    server.onchange = function(event) {
      onchange_server(event.target);
    }
  }

  var uartmode = document.getElementById("uartmode");
  if (uartmode) {
    uartmode.onchange = function(event) {
      onchange_uartmode(event.target);
    }
  }

};