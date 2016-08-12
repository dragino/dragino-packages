include $(TOPDIR)/rules.mk

PKG_NAME:=globacom-pkg
PKG_VERSION:=1.3

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=<support@dragino.com>
	DEPENDS:=+curl
endef

define Package/$(PKG_NAME)/description
	Files for globacom applications
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	# NOOP
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc	

	$(INSTALL_DIR) $(1)/etc/init.d	
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/etc/init.d/globacom $(1)/etc/init.d/globacom	
	
	$(INSTALL_DIR) $(1)/usr
	$(INSTALL_DIR) $(1)/usr/bin	
	$(CP) $(PKG_BUILD_DIR)/usr/* $(1)/usr/	
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	/usr/bin/postinst_globacom_pkg.sh
fi
exit 0
endef

$(eval $(call BuildPackage,$(PKG_NAME)))