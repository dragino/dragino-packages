#
# Copyright (c) 2013 Arduino LLC. All right reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino-yun-conf
PKG_VERSION:=1.2.0
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

define Package/dragino-yun-conf
  SECTION:=utils
  CATEGORY:=Base system
  TITLE:=Custom Dragino Yun configurations
  DEPENDS:=+base-files +triggerhappy
endef

define Package/dragino-yun-conf/conffiles
/etc/avahi/services/arduino.service
/etc/config/arduino
/etc/httpd.conf
endef

define Build/Compile
	# NOOP
endef

define Package/dragino-yun-conf/install
	$(INSTALL_DIR) $(1)/etc/avahi/services
	$(INSTALL_DIR) $(1)/etc/arduino
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/etc/triggerhappy/triggers.d

	$(INSTALL_CONF) ./files/etc/avahi/services/arduino.service $(1)/etc/avahi/services/
	$(INSTALL_CONF) ./files/etc/triggerhappy/triggers.d/reset.conf $(1)/etc/triggerhappy/triggers.d

	$(INSTALL_CONF) ./files/etc/httpd.conf $(1)/etc/
	$(INSTALL_CONF) ./files/etc/config/arduino $(1)/etc/config/
	$(INSTALL_CONF) ./files/etc/config/firewall $(1)/etc/config/firewall.1
	#$(INSTALL_CONF) ./files/etc/config/dhcp $(1)/etc/config/dhcp.1

	$(INSTALL_CONF) ./files/etc/arduino/linino-arduino-release $(1)/etc/arduino/
	sed -i "s/DATE/`date`/g" $(1)/etc/arduino/linino-arduino-release
	$(INSTALL_CONF) ./files/etc/arduino/gpg_gen_key_batch $(1)/etc/arduino/
	$(INSTALL_DATA) ./files/etc/arduino/wifi_timezones.csv.gz $(1)/etc/arduino
	$(INSTALL_DATA) ./files/etc/arduino/wifi_error_statuses.csv.gz $(1)/etc/arduino
	$(INSTALL_DATA) ./files/etc/arduino/wifi_error_reasons.csv.gz $(1)/etc/arduino

	$(INSTALL_DATA) ./files/etc/LININO_CONTRIBUTORS.TXT $(1)/etc/
endef

$(eval $(call BuildPackage,dragino-yun-conf))
