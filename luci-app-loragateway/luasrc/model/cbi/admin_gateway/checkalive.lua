 m = Map("system", translate("Network checkalive"), translate("Configuration to check network if alive"))
 s = m:section(NamedSection, "netalive", "Network", translate("Network checking alive"))

 local checkalive = s:option(Flag, "checkalive", translate("Enabled checking"))
 checkalive.enable = "1"
 checkalive.disable = "0"
 checkalive.default = checkalive.disable

 local pinghost = s:option(Value, "pinghost", translate("Ping hostname"))
 pinghost.placeholder = "localhost"

 local debug = s:option(Flag, "debugmsg", translate("Enabled debugmsg"))
 debug.enable = "1"
 debug.disable = "0"
 debug.default = debug.disable

 return m
