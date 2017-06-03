# Copyright (c) 2013 Edwin Chen

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino2
PKG_VERSION:=1.4

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/dragino2
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=
  TITLE:=Dragino -- OpenWrt Sensor IoT Project
  URL:=http://www.dragino.com
  MAINTAINER:=Edwin Chen <edwin@dragino.com>
endef

define Package/dragino2/description
	Code for dragino2 project.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/etc
	mkdir -p $(PKG_BUILD_DIR)/lib
	mkdir -p $(PKG_BUILD_DIR)/usr/bin
	mkdir -p $(PKG_BUILD_DIR)/www
	mkdir -p $(PKG_BUILD_DIR)/usr/lib/lua/dragino/iot
	$(CP) ./files/etc/* $(PKG_BUILD_DIR)/etc
	$(CP) ./files/usr/* $(PKG_BUILD_DIR)/usr
	$(CP) ./files/lib/* $(PKG_BUILD_DIR)/lib
	$(CP) ./files/www/* $(PKG_BUILD_DIR)/www
endef

define Build/Compile
endef

define Package/dragino2/install
	$(INSTALL_DIR) $(1)/etc
	cp -f $(PKG_BUILD_DIR)/etc/* $(1)/etc/
	
	$(INSTALL_DIR) $(1)/etc/triggerhappy
	$(CP) $(PKG_BUILD_DIR)/etc/triggerhappy/* $(1)/etc/triggerhappy/
	
	$(INSTALL_DIR) $(1)/etc/cellular
	$(CP) $(PKG_BUILD_DIR)/etc/cellular/* $(1)/etc/cellular/
	
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(CP) $(PKG_BUILD_DIR)/etc/uci-defaults/* $(1)/etc/uci-defaults/	
	
	$(INSTALL_DIR) $(1)/etc/avahi
	$(CP) $(PKG_BUILD_DIR)/etc/avahi/* $(1)/etc/avahi/	

	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/etc/config/* $(1)/etc/config/	

	$(INSTALL_DIR) $(1)/etc/opkg
	$(CP) $(PKG_BUILD_DIR)/etc/opkg/* $(1)/etc/opkg/	

	$(INSTALL_DIR) $(1)/etc/rc.d
	$(CP) $(PKG_BUILD_DIR)/etc/rc.d/* $(1)/etc/rc.d/	

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/etc/init.d/??* $(1)/etc/init.d/

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib/lua/dragino/iot
	$(CP) $(PKG_BUILD_DIR)/usr/* $(1)/usr/

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/*.sh $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/functions2.sh $(1)/lib/functions2.sh
	
	$(INSTALL_DIR) $(1)/lib/batman-adv
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/batman-adv/config.sh $(1)/lib/batman-adv/config.sh

	$(INSTALL_DIR) $(1)/www
	$(CP) $(PKG_BUILD_DIR)/www/* $(1)/www/
endef

$(eval $(call BuildPackage,dragino2))
