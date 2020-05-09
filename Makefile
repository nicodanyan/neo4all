CHAINPREFIX := /opt/mipsel-RetroFW-linux-uclibc
CROSS_COMPILE := $(CHAINPREFIX)/usr/bin/mipsel-linux-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

include config.mk

TARGET   = neo4all/neo4all.dge

DEFAULT_CFLAGS = $(SDL_CFLAGS) -D_GNU_SOURCE=1 -D_REENTRANT -DDINGUX -DDINGOO -DRETROFW -DLOWERCASEFILES -DDATA_PREFIX=\"data/\" -DROM_PREFIX=\"roms\" -DMEMCARD_PREFIX=\"/tmp/\"

LDFLAGS = $(SDL_LIBS) -lSDL -lSDL_mixer -lSDL_image -lz -lpthread
#-s -static

MORE_CFLAGS = -Isrc/ -Isrc/include/ -Isrc/menu -Isrc/vkbd
MORE_CFLAGS += -Wno-unused -Wno-format
MORE_CFLAGS += -DUSE_SDL -DGCCCONSTFUNC="__attribute__((const))" -DUSE_UNDERSCORE
MORE_CFLAGS += -DUNALIGNED_PROFITABLE -DREGPARAM="__attribute__((regparm(3)))" -DOPTIMIZED_FLAGS -D__inline__=__inline__
MORE_CFLAGS += -Isrc -Isrc/68k -Isrc/video -Isrc/68k/fame/

MORE_CFLAGS += -falign-functions -falign-loops -falign-labels -falign-jumps
MORE_CFLAGS += -ffast-math -fsingle-precision-constant -funsafe-math-optimizations
MORE_CFLAGS += -fomit-frame-pointer -fno-builtin -fno-exceptions -fno-common
MORE_CFLAGS += -fstrict-aliasing  -fexpensive-optimizations -fno-rtti
MORE_CFLAGS += -finline -finline-functions -fpeel-loops

MORE_CFLAGS += -O3 -mips32 -mtune=mips32 -mno-mips16 -mno-shared -mbranch-likely -pipe

CFLAGS  = $(DEFAULT_CFLAGS) $(NEO4ALL_CFLAGS) $(MORE_CFLAGS)
CPPFLAGS = $(CFLAGS)

all: $(OBJS) $(Z80_OBJS) $(MEMORY_OBJ) $(OBJS_68K) src/dingoo.o
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(OBJS_68K) $(MEMORY_OBJ) $(Z80_OBJS) src/dingoo.o $(LDFLAGS)
	$(STRIP) $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) src/dingoo.o src/68k/c68k/cpu68k.o src/z80/z80intrf.o src/z80/raze/raze1.asm $(MEMORY_OBJ)
	rm -f $(Z80_OBJS) $(OBJS_68K)

opk: all
	@mksquashfs \
	neo4all/default.retrofw.desktop \
	neo4all/neogeo.retrofw.desktop \
	neo4all/neo4all.dge \
	neo4all/neo4all.png \
	neo4all/neo4all.man.txt \
	neo4all/data \
	neo4all/neo4all.opk \
	-all-root -noappend -no-exports -no-xattrs
