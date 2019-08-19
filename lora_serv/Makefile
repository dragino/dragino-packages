include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_lorawan_server
PKG_VERSION:=1.0.2
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

TARGET_LDFLAGS:= -lsqlite3 -lpthread  

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=Dragino <support@dragino.com>
	DEPENDS:=+libc +libsqlite3 +libpthread 
endef

define Package/$(PKG_NAME)/description
	LoRaWan simple server
	For Dragino LG01N/OLG01N/LG02/OLG02/LG308/LPS8
endef

define Package/$(PKG_NAME)/extra_provides
	echo 'libc.so.6';\
	echo 'libpthread.so.0';
endef

CONFIGURE_VARS+= \
	CC="$(TOOLCHAIN_DIR)/bin/$(TARGET_CC)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/dls_utili $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
