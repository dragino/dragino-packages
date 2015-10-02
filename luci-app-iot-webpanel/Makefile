#
# Copyright (c) 2014 Dragino All right reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-iot-webpanel
PKG_VERSION:=Dragino-Yun-2.0
PKG_RELEASE:=1


PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-iot-webpanel
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:= Dragino IOT web panel
  DEPENDS:=+luci +luci-lib-json +uhttpd-mod-lua
endef

define Package/luci-app-iot-webpanel/description
	dragino iot web panel
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/usr
	mkdir -p $(PKG_BUILD_DIR)/www
	mkdir -p $(PKG_BUILD_DIR)/etc/config
	$(CP) ./files/usr/* $(PKG_BUILD_DIR)/usr
	$(CP) ./files/www/* $(PKG_BUILD_DIR)/www
	$(CP) ./files/etc/* $(PKG_BUILD_DIR)/etc
endef

define Build/Compile
	# NOOP
endef

define Package/luci-app-iot-webpanel/install
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc/

	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/arduino
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/arduino
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/dragino
	$(INSTALL_DIR) $(1)/www/luci-static/resources/arduino
	$(INSTALL_DIR) $(1)/www/luci-static/resources/dragino

	#find $(PKG_BUILD_DIR)/usr/ -name '*.lua' -exec luac -s -o {} {} \;
	$(CP) $(PKG_BUILD_DIR)/usr/* $(1)/usr/
	$(CP) $(PKG_BUILD_DIR)/www/* $(1)/www/
endef

$(eval $(call BuildPackage,luci-app-iot-webpanel))

