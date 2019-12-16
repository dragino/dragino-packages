# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_gw
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
SRCDIR=$(PKG_BUILD_DIR)/src
PAHO_SRC=$(PKG_BUILD_DIR)/paho.mqtt.embedded-c

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/lora-gateway/Default
  TITLE:=Dragino lora-gateway package
  URL:=http://www.dragino.com
endef

define Package/$(PKG_NAME)
$(call Package/lora-gateway/Default)
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libftdi +libprotobuf-c 
endef

define Package/$(PKG_NAME)/description
  dragino-gw is a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

TARGET_CFLAGS += -Wextra -std=c99 -Iinc -I. -lm 

TARGET_CFLAGS += $(FPIC) -Wall -g -O2 -I$(SRCDIR) -I$(PAHO_SRC)/MQTTClient-C/src -I$(PAHO_SRC)/MQTTPacket/src -I$(SRCDIR)/github.com/gogo/protobuf/protobuf -I$(SRCDIR)/github.com/TheThingsNetwork -DMQTT_TASK $(shell pkg-config --cflags 'libprotobuf-c >= 1.0.0')

TARGET_LDFLAGS += -lpthread -lpaho-embed-mqtt3c $(shell pkg-config --libs 'libprotobuf-c >= 1.0.0')

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/lora-gateway
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lora_gateway/libloragw/inc/* $(1)/usr/include/lora-gateway
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lora_gateway/libloragw/libloragw.a $(1)/usr/lib/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/paho.mqtt.embedded-c/build/output/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1.0 $(1)/usr/lib/libpaho-embed-mqtt3c.so.1
	$(LN) $(1)/usr/lib/libpaho-embed-mqtt3c.so.1 $(1)/usr/lib/libpaho-embed-mqtt3c.so
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ttn-gateway-connector/bin/libttn-gateway-connector.so $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dragino-gw-fwd/dragino_gw_fwd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lora_gateway/reset_lgw.sh $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/lora
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/dragino-gw-fwd/*conf.json $(1)/etc/lora
	$(INSTALL_DIR) $(1)/etc/lora/cfg
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/dragino-gw-fwd/conf/*.json* $(1)/etc/lora/cfg
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
