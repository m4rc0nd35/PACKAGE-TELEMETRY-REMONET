#
# Copyright (C) 2021 M4rc0nd35 <dev@remonet.com.br>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=telemetry
PKG_VERSION:=0.32.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL
PKG_LICENSE_FILES:=COPYING

PKG_BUILD_PARALLEL:=1
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/telemetry/Default
	SECTION:=utils
	CATEGORY:=Utilities
	PKGARCH:=all
	MAINTAINER:=J34n M4rc0nd35 <dev@remonet.com.br>
endef

define Package/telemetry/description
	Dependencies and scripts for telemetry/commands with Remonet server
endef

define Package/telemetry
	$(call Package/telemetry/Default)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Install application telemetry/commands Remonet server
	DEPENDS:=+px5g-mbedtls \
		+libustream-mbedtls \
		+libmbedtls
	MENU:=1
endef

define Build/Compile
  $(MAKE) -C $(PKG_BUILD_DIR) \
          CC="$(TARGET_CC)" \
          CFLAGS="$(TARGET_CFLAGS) -Wall" \
          LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/telemetry/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/remonet $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pk $(1)/usr/bin/
endef

$(eval $(call BuildPackage,telemetry))
