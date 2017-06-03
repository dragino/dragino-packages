#
# Copyright (c) 2014 Arduino LLC. All right reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=cpu-mcu-bridge
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=CPU-MCU Bridge
	DEPENDS:=+python-light
endef

define Package/$(PKG_NAME)/description
	Bridge lets linux and arduino communicate
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./dragino-yun-bridge/??* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/python2.7/bridge
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/bridge/*.py $(1)/usr/lib/python2.7/bridge

	$(INSTALL_DIR) $(1)/usr/lib/php/bridge
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/bridge/php/*.php $(1)/usr/lib/php/bridge
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
