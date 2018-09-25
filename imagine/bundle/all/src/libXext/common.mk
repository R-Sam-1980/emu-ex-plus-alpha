ifndef CHOST
CHOST := $(shell cc -dumpmachine)
else
buildArg := --build=$(shell cc -dumpmachine)
endif

buildArg += xorg_cv_malloc0_returns_null=yes

libXextVer := 1.3.3
libXextSrcDir := libXext-$(libXextVer)
libXextSrcArchive := libXext-$(libXextVer).tar.bz2

makeFile := $(buildDir)/Makefile
outputLibFile := $(buildDir)/src/.libs/libXext.a
installIncludeDir := $(installDir)/include/X11/extensions

all : $(outputLibFile)

install : $(outputLibFile)
	@echo "Installing libXext to: $(installDir)"
	@mkdir -p $(installIncludeDir) $(installDir)/lib/pkgconfig
	cp $(outputLibFile) $(installDir)/lib/
	cp $(libXextSrcDir)/include/X11/extensions/*.h $(installIncludeDir)/
	cp $(buildDir)/xext.pc $(installDir)/lib/pkgconfig/

.PHONY : all install

$(libXextSrcDir)/configure : | $(libXextSrcArchive)
	@echo "Extracting libXext..."
	tar -mxjf $|
	cd $(libXextSrcDir) && autoreconf -isf

$(outputLibFile) : $(makeFile)
	@echo "Building libXext..."
	$(MAKE) -j4 -C $(<D)

$(makeFile) : $(libXextSrcDir)/configure
	@echo "Configuring libXext..."
	@mkdir -p $(@D)
	dir=`pwd` && cd $(@D) && CC="$(CC)" CFLAGS="$(CPPFLAGS) $(CFLAGS)" LD="$(LD)" LDFLAGS="$(LDLIBS)" $$dir/$(libXextSrcDir)/configure --prefix=$(installDir) --disable-shared --disable-specs --host=$(CHOST) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) PKG_CONFIG=pkg-config $(buildArg)

