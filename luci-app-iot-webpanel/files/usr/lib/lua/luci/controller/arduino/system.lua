--[[
    Copyright (C) 2014 Dragino Technology Co., Limited

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
]]--

module("luci.controller.arduino.system", package.seeall)

function index()
	entry({ "webpanel", "flashops" }, call("action_flashops"), nil)
end

function fork_exec(command)
	local pid = nixio.fork()
	if pid > 0 then
		return
	elseif pid == 0 then
		-- change to root dir
		nixio.chdir("/")

		-- patch stdin, out, err to /dev/null
		local null = nixio.open("/dev/null", "w+")
		if null then
			nixio.dup(null, nixio.stderr)
			nixio.dup(null, nixio.stdout)
			nixio.dup(null, nixio.stdin)
			if null:fileno() > 2 then
				null:close()
			end
		end

		-- replace with target command
		nixio.exec("/bin/sh", "-c", command)
	end
end

function action_flashops()
        local sys = require "luci.sys"
        local fs  = require "luci.fs"

        local upgrade_avail = nixio.fs.access("/lib/upgrade/platform.sh")
        local reset_avail   = os.execute([[grep '"rootfs_data"' /proc/mtd >/dev/null 2>&1]]) == 0

        local restore_cmd = "tar -xzC/ >/dev/null 2>&1"
        local backup_cmd  = "sysupgrade --create-backup - 2>/dev/null"
        local image_tmp   = "/tmp/firmware.img"

        local function image_supported()
                -- XXX: yay...
                return ( 0 == os.execute(
                        ". /lib/functions.sh; " ..
                        "include /lib/upgrade; " ..
                        "platform_check_image %q >/dev/null"
                                % image_tmp
                ) )
        end

        local function image_checksum()
                return (luci.sys.exec("md5sum %q" % image_tmp):match("^([^%s]+)"))
        end

        local function storage_size()
                local size = 0
                if nixio.fs.access("/proc/mtd") then
                        for l in io.lines("/proc/mtd") do
                                local d, s, e, n = l:match('^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s+"([^%s]+)"')
                                if n == "linux" or n == "firmware" then
                                        size = tonumber(s, 16)
                                        break
                                end
                        end
                elseif nixio.fs.access("/proc/partitions") then
                        for l in io.lines("/proc/partitions") do
                                local x, y, b, n = l:match('^%s*(%d+)%s+(%d+)%s+([^%s]+)%s+([^%s]+)')
                                if b and n and not n:match('[0-9]') then
                                        size = tonumber(b) * 1024
                                        break
                                end
                        end
                end
                return size
        end


        local fp
        luci.http.setfilehandler(
                function(meta, chunk, eof)
                        if not fp then
                                if meta and meta.name == "image" then
                                        fp = io.open(image_tmp, "w")
                                else
                                        fp = io.popen(restore_cmd, "w")
                                end
                        end
                        if chunk then
                                fp:write(chunk)
                        end
                        if eof then
                                fp:close()
                        end
                end
        )

        if luci.http.formvalue("backup") then
                --
                -- Assemble file list, generate backup
                --
                local reader = ltn12_popen(backup_cmd)
                luci.http.header('Content-Disposition', 'attachment; filename="backup-%s-%s.tar.gz"' % {
                        luci.sys.hostname(), os.date("%Y-%m-%d")})
                luci.http.prepare_content("application/x-targz")
                luci.ltn12.pump.all(reader, luci.http.write)
        elseif luci.http.formvalue("restore") then
                --
                -- Unpack received .tar.gz
                --
                local upload = luci.http.formvalue("archive")
                if upload and #upload > 0 then
                        luci.template.render("admin_system/applyreboot")
                        luci.sys.reboot()
                end
        elseif luci.http.formvalue("image") or luci.http.formvalue("step") then
                --
                -- Initiate firmware flash
                --
                local step = tonumber(luci.http.formvalue("step") or 1)
                if step == 1 then
                        if image_supported() then
                                luci.template.render("dragino/upgrade", {
                                        checksum = image_checksum(),
                                        storage  = storage_size(),
                                        size     = nixio.fs.stat(image_tmp).size,
                                        keep     = (not not luci.http.formvalue("keep"))
                                })
                        else
                                nixio.fs.unlink(image_tmp)
                                luci.template.render("dragino/upgrade_firmware", {
                                        reset_avail   = reset_avail,
                                        upgrade_avail = upgrade_avail,
                                        image_invalid = true
                                })
                        end
                --
                -- Start sysupgrade flash
                --
                elseif step == 2 then
                        local keep = (luci.http.formvalue("keep") == "1") and "" or "-n"
                        luci.template.render("dragino/upgrading", {
                               addr  = (#keep > 0) and "192.168.1.1" or nil
                        })
                        fork_exec("killall dropbear uhttpd; sleep 1; /sbin/sysupgrade %s %q" %{ keep, image_tmp })
                end
        elseif reset_avail and luci.http.formvalue("reset") then
                --
                -- Reset system
                --
                luci.template.render("admin_system/applyreboot", {
                        title = luci.i18n.translate("Erasing..."),
                        msg   = luci.i18n.translate("The system is erasing the configuration partition now and will reboot itself when finished."),
                        addr  = "192.168.1.1"
                })
                fork_exec("killall dropbear uhttpd; sleep 1; mtd -r erase rootfs_data")
        else
                --
                -- Overview
                --
                luci.template.render("dragino/upgrade_firmware", {
                        reset_avail   = reset_avail,
                        upgrade_avail = upgrade_avail
                })
        end
end