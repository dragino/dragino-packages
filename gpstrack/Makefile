include $(TOPDIR)/rules.mk

PKG_NAME:=gpstrack
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
	A gps tracking project 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	chmod +x ./src/*.sh
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/gpstrack $(1)/etc/config
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/*.sh $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
	$(CP) $(PKG_BUILD_DIR)/gpstrack.lua $(1)/usr/lib/lua/luci/controller/admin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/gpstrack
	$(CP) $(PKG_BUILD_DIR)/gpswox.lua $(1)/usr/lib/lua/luci/model/cbi/gpstrack

endef

$(eval $(call BuildPackage,$(PKG_NAME)))
