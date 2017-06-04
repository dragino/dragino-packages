--[[
right 2008 Steven Barth <steven@midlink.org>
Copyright 2008-2011 Jo-Philipp Wich <xm@subsignal.org>
Copyright 2014 Edwin Chen <edwin@dragino.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

$Id$
]]--

module("luci.controller.admin.gpstrack", package.seeall)

function index()
	entry({"admin", "sensor", "gps"}, cbi("admin_sensor/gps"), _("GPS Track Server"), 6)
end
