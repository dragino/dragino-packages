# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_gw_fwd
PKG_VERSION:=1.2.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
    TITLE:=Dragino lora-gateway forward
    URL:=http://www.dragino.com
    MAINTAINER:=dragino
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
    SECTION:=utils
    CATEGORY:=Utilities
	DEPENDS:=+libsqlite3 +libmpsse
endef

define Package/$(PKG_NAME)/description
  Dragino-gw is a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/lora-gateway
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/hal/*.h $(1)/usr/include/lora-gateway
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/hal/libloragw.a $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libmqtt/libpahomqtt3c.so $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/lib
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fwd/fwd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hal/rx_test $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hal/tx_test $(1)/usr/bin
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/reset_lgw.sh $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/lora
	#$(INSTALL_CONF) $(PKG_BUILD_DIR)/config/gwcfg.json $(1)/etc/lora/local_conf.json
	#$(INSTALL_CONF) $(PKG_BUILD_DIR)/config/sxcfg.json $(1)/etc/lora/global_conf.json
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libmqtt/libpahomqtt3c.so $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc/lora/cfg
	$(CP) $(PKG_BUILD_DIR)/config/*json* $(1)/etc/lora/cfg
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
