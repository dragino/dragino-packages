#
# Copyright (c) 2014 Dragino All right reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=helium_gateway
PKG_VERSION:=1.2.1
PKG_RELEASE:=1


PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:= Dragino data-only hostpot
  DEPENDS:=+uhttpd-mod-lua
  URL:=http://www.dragino.com
  MAINTAINER:=Dragino <support@dragino.com>
endef

define Package/$(PKG_NAME)/description
	Dragino data-only hostpot
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/usr
	mkdir -p $(PKG_BUILD_DIR)/usr/bin
	mkdir -p $(PKG_BUILD_DIR)/etc
	mkdir -p $(PKG_BUILD_DIR)/etc/init.d
	mkdir -p $(PKG_BUILD_DIR)/etc/helium_gateway
	$(CP) ./etc/init.d/* $(PKG_BUILD_DIR)/etc/init.d
	$(CP) ./etc/helium_gateway/* $(PKG_BUILD_DIR)/etc/helium_gateway
	$(CP) ./usr/bin/* $(PKG_BUILD_DIR)/usr/bin
endef

define Package/my-package/postinst
#!/bin/sh
/etc/init.d/helium_gateway enable
/etc/init.d/helium_gateway reload
exit 0
endef

define Build/Compile
	# NOOP
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/
	$(INSTALL_DIR) $(1)/usr/
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc/
	$(CP) $(PKG_BUILD_DIR)/usr/* $(1)/usr/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

