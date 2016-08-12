include $(TOPDIR)/rules.mk

PKG_NAME:=auto-provisioning
PKG_VERSION:=1.2

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=<support@dragino.com>
	DEPENDS:=+fping +atftp +wget +ca-certificates
endef

define Package/$(PKG_NAME)/description
	Auto-provisionging for the whole system 
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
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usr/bin/auto-provisioning.sh $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usr/bin/postinst-auto-provisioning.sh $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc
	$(CP) $(PKG_BUILD_DIR)/etc/* $(1)/etc	
	
	$(INSTALL_DIR) $(1)/etc/init.d	
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/etc/init.d/provisioning $(1)/etc/init.d/provisioning
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	/usr/bin/postinst-auto-provisioning.sh
fi
exit 0
endef

$(eval $(call BuildPackage,$(PKG_NAME)))