include $(TOPDIR)/rules.mk

PKG_NAME:=tcp_client
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	DEPENDS:=+libuci +librt
	MAINTAINER:=skerlan <myEmail.add>
endef

define Package/$(PKG_NAME)/description
	This is a tcp_client package that contains a program to send senser data to a tcp server.
endef

CONFIGURE_VARS+= \
	CC="$(TOOLCHAIN_DIR)/bin/$(TARGET_CC)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/iot/scripts
	$(CP) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/etc/iot/scripts
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
