# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
# libpaho-embed-mqtt3c

include $(TOPDIR)/rules.mk

PKG_NAME:=libpaho-embed-mqtt3c
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
  TITLE:=Mqtt embed libraries for c lang
  URL:=http://www.dragino.com
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
  SECTION:=libs
  CATEGORY:=Libraries
endef

define Package/$(PKG_NAME)/description
   Eclipse Paho MQTT C/C++ client for Embedded platforms 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/build/output/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so.1
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1 $(1)/usr/lib/libpaho-embed-mqtt3c.so
endef

define Build/$(PKG_NAME)/Install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/build/output/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so.1
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1 $(1)/usr/lib/libpaho-embed-mqtt3c.so
endef

$(eval $(call BuildPackage,libpaho-embed-mqtt3c))
