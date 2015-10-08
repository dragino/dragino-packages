##############################################
# OpenWrt Makefile for a socket server controlling discrete I/O ports 
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to 
# specify a basic description of the package, 
# where to build our program, where to find 
# the source files, and where to install the 
# compiled program on the router. 
# 
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and 
# there should be no trailing whitespace in
# lines that are not commented.
# 
##############################################

include $(TOPDIR)/rules.mk

# Name and release number of this package
PKG_NAME:=socket_io
PKG_RELEASE:=1


# This specifies the directory where we're going to build the program.  
# The root build directory, $(BUILD_DIR), is by default the build_mipsel 
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)


include $(INCLUDE_DIR)/package.mk



# Specify package information for this program. 
# The variables defined here should be self explanatory.
define Package/socket_io
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+libuci
	TITLE:=Listening socket server controlling discrete I/O ports 
endef

define Package/socket_io/description
 SIP I/O Device (SIOD) is an I/O controller hardware similar to MS14/HE which 
 supports wifi mesh network and SIP protocol. Server_io is a listening socket 
 server - part of the SIOD project. 
 It accepts commands from the socket to read/write the available discrete I/O ports. 
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_LDFLAGS += -luci
# We do not need to define Build/Configure or Build/Compile directives
# The defaults are appropriate for compiling a simple program such as this one
#define Build/Compile
#	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS) -luci
#endef

# Specify where and how to install the program. Since we only have one file at the moment, 
# the socket_io executable, install it by copying it to the /bin directory on
# the router. The $(1) variable represents the root directory on the router running 
# OpenWrt. The $(INSTALL_DIR) variable contains a command to prepare the install 
# directory if it does not already exist.  Likewise $(INSTALL_BIN) contains the 
# command to copy the binary file from its current location (in our case the build
# directory) to the install directory.
define Package/socket_io/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/socket_io $(1)/bin/

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/siod $(1)/etc/config/siod
endef


# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.
$(eval $(call BuildPackage,socket_io))
