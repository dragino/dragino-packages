# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_gw
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
    TITLE:=Dragino lora-gateway package
    URL:=http://www.dragino.com
    MAINTAINER:=dragino
endef

define Package/libpaho-embed-mqtt3c
$(call Package/$(PKG_NAME)/Default)
    SECTION:=libs
    CATEGORY:=Libraries
endef

define Package/libttn-gateway-connector
$(call Package/$(PKG_NAME)/Default)
    SECTION:=utils
    CATEGORY:=Utilities
    DEPENDS:=+libprotobuf-c +libpaho-embed-mqtt3c 
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
    SECTION:=utils
    CATEGORY:=Utilities
endef

define Package/$(PKG_NAME)/description
  Dragino-gw is a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

define Package/libttn-gateway-connector/extra_provides
	echo 'libpaho-embed-mqtt3c.so.1';
endef

define Package/$(PKG_NAME)/extra_provides
	echo 'libprotobuf-c.so.1';\
	echo 'libttn-gateway-connector.so';\
	echo 'libpaho-embed-mqtt3c.so.1';
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/lora-gateway
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libloragw/inc/* $(1)/usr/include/lora-gateway
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/paho.mqtt.embedded-c/build/output/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so.1
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libloragw/libloragw.a $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ttn-gateway-connector/bin/libttn-gateway-connector.so $(1)/usr/lib
endef

define Build/libpaho-embed-mqtt3c/Install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/paho.mqtt.embedded-c/build/output/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so.1
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1 $(1)/usr/lib/libpaho-embed-mqtt3c.so
endef

define Package/libttn-gateway-connector/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ttn-gateway-connector/bin/libttn-gateway-connector.so $(1)/usr/lib
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dragino-gw-fwd/dragino_gw_fwd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/reset_lgw.sh $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/lora
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/dragino-gw-fwd/*conf.json $(1)/etc/lora
	$(INSTALL_DIR) $(1)/etc/lora/cfg
	$(CP) $(PKG_BUILD_DIR)/dragino-gw-fwd/config/??* $(1)/etc/lora/cfg
endef

$(eval $(call BuildPackage,libpaho-embed-mqtt3c))
$(eval $(call BuildPackage,libttn-gateway-connector))
$(eval $(call BuildPackage,$(PKG_NAME)))
