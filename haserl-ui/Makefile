#
# Copyright (c) 2014 Dragino All right reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=haserl-ui
PKG_VERSION:=1.5.1
PKG_RELEASE:=0


PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:= Dragino loragateway web panel
  DEPENDS:=+uhttpd-mod-lua
  URL:=http://www.dragino.com
  MAINTAINER:=Dragino <info@dragino.com>
endef

define Package/$(PKG_NAME)/description
	dragino loragateway web panel
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/usr
	mkdir -p $(PKG_BUILD_DIR)/bin
	mkdir -p $(PKG_BUILD_DIR)/etc
	mkdir -p $(PKG_BUILD_DIR)/etc/iot
	mkdir -p $(PKG_BUILD_DIR)/etc/iot/cert
	mkdir -p $(PKG_BUILD_DIR)/etc/config
	mkdir -p $(PKG_BUILD_DIR)/usr/lib//lua/dragino
	mkdir -p $(PKG_BUILD_DIR)/www/cgi-bin/inc
	mkdir -p $(PKG_BUILD_DIR)/www/static/img
	mkdir -p $(PKG_BUILD_DIR)/www/static/resources/font-awesome/css
	mkdir -p $(PKG_BUILD_DIR)/www/static/resources/font-awesome/webfonts	
	$(CP) ./root/usr/* $(PKG_BUILD_DIR)/usr
	$(CP) ./root/etc/* $(PKG_BUILD_DIR)/etc
	$(CP) ./root/www/* $(PKG_BUILD_DIR)/www
	$(CP) ./root/bin/* $(PKG_BUILD_DIR)/bin
endef

define Build/Compile
	# NOOP
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/
	$(INSTALL_DIR) $(1)/usr/
	$(INSTALL_DIR) $(1)/bin/
	$(INSTALL_DIR) $(1)/www/
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc/
	$(CP) $(PKG_BUILD_DIR)/usr/* $(1)/usr/
	$(CP) $(PKG_BUILD_DIR)/bin/* $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/www/* $(1)/www/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

