include $(TOPDIR)/rules.mk

PKG_NAME:=fping
PKG_VERSION:=4.0
PKG_RELEASE:=1

PKG_SOURCE:=fping-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://www.fping.org/dist/
PKG_MD5SUM:=c21a80d7519fa0ad2411bf6799873eb0
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)    
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=$(PKG_NAME)
	MAINTAINER:=Mazilo <myEmail.add>
endef

define Package/$(PKG_NAME)/description
	fping is a program to send ICMP echo probes to network hosts, similar to ping, but much better performing when pinging multiple hosts.
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/$(PKG_NAME) $(1)/usr/bin/$(PKG_NAME)
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
