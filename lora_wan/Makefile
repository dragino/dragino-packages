include $(TOPDIR)/rules.mk

PKG_NAME:=loraWAN
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=Mazilo <myEmail.add>
endef

define Package/$(PKG_NAME)/description
	The loarWAN gateway connect to the things network(TTN)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/LoRaWAN $(1)/etc/config
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gwstat.lua $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/base64.lua $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/admin_sensor
	$(CP) $(PKG_BUILD_DIR)/LoRaWAN.lua $(1)/usr/lib/lua/luci/model/cbi/admin_sensor

endef

$(eval $(call BuildPackage,$(PKG_NAME)))
