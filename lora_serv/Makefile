include $(TOPDIR)/rules.mk

PKG_NAME:=lora_serv
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

TARGET_LDFLAGS:= -lc -lsqlite3 -lpthread  

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
	For Dragino LG01/LG02/LG08
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
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
