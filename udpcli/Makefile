include $(TOPDIR)/rules.mk

PKG_NAME:=udpcli
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=edwin <edwin@dragino.com>
endef

define Package/$(PKG_NAME)/description
	UDP tool to send and get udp message
endef

CONFIGURE_VARS+= \
	CC="$(TOOLCHAIN_DIR)/bin/$(TARGET_CC)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/udplistener $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/udpcli $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))