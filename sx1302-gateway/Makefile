# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=sx1302-gateway
PKG_VERSION:=1.0.1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
  TITLE:=Semtech SX1302 lora-gateway package
  URL:=http://www.semtech.com/wireless-rf/lora.html
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libftdi 
endef

define Package/$(PKG_NAME)/description
  sx1302-gateway is a library to build a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/$(PKG_NAME)
	$(CP) $(PKG_BUILD_DIR)/libloragw/inc/* $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libloragw/libloragw.a $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lora
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libloragw/test* $(1)/usr/lora
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/reset_lgw.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/packet_forwarder/lora_pkt_fwd $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/lora
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/packet_forwarder/global_conf.json $(1)/etc/lora
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
