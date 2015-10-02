include $(TOPDIR)/rules.mk

PKG_NAME:=auto-update-packages
PKG_VERSION:=1.0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=<support@dragino.com>
	DEPENDS:=+fping
endef

define Package/$(PKG_NAME)/description
	Package for auto update selected packages. 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	# NOOP
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin	
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usr/bin/update_packages.sh $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc	
endef

$(eval $(call BuildPackage,$(PKG_NAME)))