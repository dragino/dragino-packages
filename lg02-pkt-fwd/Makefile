include $(TOPDIR)/rules.mk

PKG_NAME:=lg02_pkt_fwd
PKG_VERSION:=1.1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

TARGET_LDFLAGS:= -lpthread  

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=Dragino <support@dragino.com>
	DEPENDS:=+libuci +libpthread +librt
endef

define Package/$(PKG_NAME)/description
	LoRa dual channels packet forwarder
	For Dragino LG02/OLG02
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
	$(CP) $(PKG_BUILD_DIR)/lg02_single_rx_tx $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
