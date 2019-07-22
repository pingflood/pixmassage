CHAINPREFIX=/opt/mipsel-linux-uclibc
CROSS_COMPILE=$(CHAINPREFIX)/usr/bin/mipsel-linux-

BUILDTIME=$(shell date +'\"%Y-%m-%d %H:%M\"')

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT     := $(CHAINPREFIX)/usr/mipsel-buildroot-linux-uclibc/sysroot
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS = -DTARGET_RETROFW -D__BUILDTIME__="$(BUILDTIME)" -DLOG_LEVEL=0 -g0 -Os $(SDL_CFLAGS) -I$(CHAINPREFIX)/usr/include/ -I$(SYSROOT)/usr/include/  -I$(SYSROOT)/usr/include/SDL/ -mhard-float -mips32 -mno-mips16
CFLAGS += -std=c++11 -fdata-sections -ffunction-sections -fno-exceptions -fno-math-errno -fno-threadsafe-statics

LDFLAGS = $(SDL_LIBS) -lfreetype -lSDL_image -lSDL_ttf -lSDL -lpthread
LDFLAGS +=-Wl,--as-needed -Wl,--gc-sections -s

pc:
	gcc pixmassage.c -g -o pixmassage.dge -ggdb -O0 -DDEBUG -lSDL_image -lSDL -lSDL_ttf -I/usr/include/SDL

retrogame:
	$(CXX) $(CFLAGS) $(LDFLAGS) pixmassage.c -o pixmassage.dge

ipk: retrogame
	@rm -rf /tmp/.pixmassage-ipk/ && mkdir -p /tmp/.pixmassage-ipk/root/home/retrofw/apps/pixmassage /tmp/.pixmassage-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@cp -r pixmassage.dge pixmassage.png /tmp/.pixmassage-ipk/root/home/retrofw/apps/pixmassage
	@cp pixmassage.lnk /tmp/.pixmassage-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" control > /tmp/.pixmassage-ipk/control
	@tar --owner=0 --group=0 -czvf /tmp/.pixmassage-ipk/control.tar.gz -C /tmp/.pixmassage-ipk/ control
	@tar --owner=0 --group=0 -czvf /tmp/.pixmassage-ipk/data.tar.gz -C /tmp/.pixmassage-ipk/root/ .
	@echo 2.0 > /tmp/.pixmassage-ipk/debian-binary
	@ar r pixmassage.ipk /tmp/.pixmassage-ipk/control.tar.gz /tmp/.pixmassage-ipk/data.tar.gz /tmp/.pixmassage-ipk/debian-binary

clean:
	rm -rf pixmassage.dge pixmassage.ipk
