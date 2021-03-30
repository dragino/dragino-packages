# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_gw_fwd
PKG_VERSION:=2.0.1
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
	DEPENDS:=+libsqlite3 
endef

define Package/$(PKG_NAME)/description
  Dragino-gw is a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

define Package/$(PKG_NAME)/extra_provides
	echo 'libmbedcrypto.so.0';\
	echo 'libmbedcrypto.so.1';\
	echo 'libmbedtls.so.10';\
	echo 'libsx1301hal.so';\
	echo 'libsx1302hal.so';\
	echo 'libmbedtls.so.12';\
	echo 'libmbedcrypto.so.3';\
	echo 'libmbedx509.so.0'; 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/??* $(1)/usr/bin
	$(LN) test_loragw_hal_rx $(1)/usr/bin/rx_test
	$(LN) test_loragw_hal_tx $(1)/usr/bin/tx_test

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/*.so $(1)/usr/lib
	$(LN) libsx1302hal.so $(1)/usr/lib/libloragw.so

	$(INSTALL_DIR) $(1)/etc/station
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/config/station/*.conf $(1)/etc/station/

	$(INSTALL_DIR) $(1)/etc/lora/cfg
	$(CP) $(PKG_BUILD_DIR)/config/fwd/??* $(1)/etc/lora/cfg

	$(INSTALL_DIR) $(1)/etc/lora
	$(CP) $(PKG_BUILD_DIR)/config/fwd/global_conf.json $(1)/etc/lora
	$(CP) $(PKG_BUILD_DIR)/config/fwd/local_conf.json $(1)/etc/lora
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
