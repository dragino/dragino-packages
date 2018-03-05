include $(TOPDIR)/rules.mk

PKG_NAME:=lg01_pkt_fwd
PKG_VERSION:=2.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

TARGET_LDFLAGS:= -lpthread  

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=Mazilo <myEmail.add>
	DEPENDS:=+libuci +libpthread +librt
endef

define Package/$(PKG_NAME)/description
	Lora singe packeage forward
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
