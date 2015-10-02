#
# Copyright (C) 2006-2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# http://madet.my
#
# Cross Compiled by
# Mahadir Ahmad <mahadir@madet.my>
#


include $(TOPDIR)/rules.mk

PKG_NAME:=smstools3
PKG_VERSION:=3.1.15
PKG_RELEASE:=1

PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk


define Package/smstools3
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=SMS Tools 3
	URL:=http://smstools3.kekekasvi.com
endef

define Package/smstools3/description
 The SMS Server Tools 3 is a SMS Gateway 
 software which can send and receive short 
 messages through GSM modems and mobile phonesendef
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

define Package/smstools3/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/smsd $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/usr/share/smstools3/examples
	$(INSTALL_DATA) ./files/examples/* $(1)/usr/share/smstools3/examples/
	$(INSTALL_DIR) $(1)/usr/share/smstools3/scripts
	$(INSTALL_BIN) ./files/scripts/* $(1)/usr/share/smstools3/scripts/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/smstools3 $(1)/etc/init.d/ 
	$(INSTALL_DATA) ./files/examples/smsd.conf.mw7880 $(1)/etc/smsd.conf
	$(INSTALL_BIN) ./files/scripts/sendsms $(1)/usr/sbin/
endef

define Package/smstools3/preinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	echo "SMS Tools 3 for OpenWrt"
	echo "Compiled by Mahadir Ahmad <mahadir@madet.my>"
	echo "Modifed by Dragino Tech<support@dragino.com>"
	echo "Installing"
fi
exit 0
endef

define Package/smstools3/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	cp /usr/share/smstools3/examples/smsd.conf.mw7880 /etc/smsd.conf
	cp /usr/share/smstools3/scripts/sendsms /usr/sbin/
	echo Edit /etc/smsd.conf according your conf
	echo For auto start run "/etc/init.d/smstools3 enable"
fi
exit 0
endef

define Package/smstools3/prerm
#!/bin/sh
# check if we are on real system
if [ -z "$${IPKG_INSTROOT}" ]; then
        echo "Removing rc.d symlink for smstools3"
        /etc/init.d/smstools3 disable
fi
exit 0
endef

define Package/smstools3/postrm
#!/bin/sh
echo cleaning...
rm -fr /usr/share/smstools3
rm /var/log/smsd.log
rm /etc/smsd.conf
endef

$(eval $(call BuildPackage,smstools3))
