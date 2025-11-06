

#
# Makefile for gaming-server
# OpenWrt Package
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gaming-server
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/gaming-server
  SECTION:=BenQ
  CATEGORY:=BenQ
  TITLE:=Gaming Server Daemon
  SUBMENU:=Applications
  DEPENDS:=+gaming-core +libwebsockets-full +cJSON +v4l-utils
endef

define Package/gaming-server/description
  Gaming Server Daemon for PS5 state monitoring and control.

  Features:
  - CEC monitoring for PS5 power state
  - Network detection for PS5 location
  - PS5 wake via CEC
  - WebSocket server for client queries
  - State machine coordination
endef

# 修正：使用 $(CP) 複製整個目錄
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

# 單行編譯（與 gaming-client 一致）
define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/gaming \
		-o $(PKG_BUILD_DIR)/gaming-server \
		$(PKG_BUILD_DIR)/main.c \
		$(PKG_BUILD_DIR)/cec_monitor.c \
		$(PKG_BUILD_DIR)/ps5_detector.c \
                $(PKG_BUILD_DIR)/ps5_wake.c \
		$(PKG_BUILD_DIR)/websocket_server.c \
		$(PKG_BUILD_DIR)/server_state_machine.c \
		$(TARGET_LDFLAGS) \
		-L$(STAGING_DIR)/usr/lib \
		-L$(STAGING_DIR)/root-mediatek/usr/lib \
		-lgaming-core \
		-lwebsockets \
		-lcjson \
		-lm \
		-lpthread
endef

define Package/gaming-server/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gaming-server $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/gaming-server $(1)/etc/config/

endef

define Package/gaming-server/conffiles
/etc/config/gaming-server
endef



define Package/gaming-client/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
        # Reload UCI configuration
        uci commit gaming-server

        # The gaming-server will be started by /etc/init.d/gaming
        # when device type is detected as 'client'
        echo "Gaming Server installed. Will start automatically on server device."
}
exit 0
endef

define Package/gaming-server/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
        # Stop the service if running
        /etc/init.d/gaming stop 2>/dev/null || true
}
exit 0
endef


$(eval $(call BuildPackage,gaming-server))
