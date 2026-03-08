TARGET = PSPTube
OBJS   = src/main.o src/graphics.o src/network.o src/youtube.o \
         src/config.o src/ui.o src/video.o

# PSP SDK paths (set by psp-config)
PSPSDK  = $(shell psp-config --pspsdk-path)
PSPDIR  = $(shell psp-config --psp-prefix)

# Compiler and tools
CC      = psp-gcc
AR      = psp-ar
RANLIB  = psp-ranlib
STRIP   = psp-strip
MKSFO   = mksfoex
PACK_PBP = pack-pbp

# Paths
INCDIR  = $(PSPSDK)/include
LIBDIR  = $(PSPSDK)/lib

# Compiler flags (NO BARE PATHS!)
CFLAGS  = -O2 -Wall -G0 \
          -I$(INCDIR) \
          -I$(PSPDIR)/include \
          -Isrc \
          -D_PSP_FW_VERSION=600

# Linker flags
LDFLAGS = -L$(LIBDIR) \
          -L$(PSPDIR)/lib \
          -Wl,-zmax-page-size=128

# Libraries (PSP specific)
LIBS    = -lpsphttp -lpspssl -lpspnet_apctl -lpspnet_inet -lpspnet \
          -lpsputility -lpspmpeg -lpspatrac3plus \
          -lpspaudio -lpspgu -lpspge -lpspdisplay \
          -lpspctrl -lpsppower -lpspsdk -lc -lm \
          -lpspdebug

# PSP EBOOT parameters
PSP_EBOOT_TITLE = PSPTube
PSP_EBOOT_ICON  = assets/ICON0.PNG
PSP_EBOOT_PIC1  = assets/PIC1.PNG

# Default target
all: $(TARGET).elf EBOOT.PBP

# Link ELF executable
$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# Compile source files
src/main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

src/graphics.o: src/graphics.c
	$(CC) $(CFLAGS) -c $< -o $@

src/network.o: src/network.c
	$(CC) $(CFLAGS) -c $< -o $@

src/youtube.o: src/youtube.c
	$(CC) $(CFLAGS) -c $< -o $@

src/config.o: src/config.c
	$(CC) $(CFLAGS) -c $< -o $@

src/ui.o: src/ui.c
	$(CC) $(CFLAGS) -c $< -o $@

src/video.o: src/video.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create SFO (System File Object)
PARAM.SFO: $(TARGET).elf
	$(MKSFO) -d MEMSIZE=1 '$(PSP_EBOOT_TITLE)' $@

# Create EBOOT.PBP
EBOOT.PBP: PARAM.SFO $(TARGET).elf
	$(PACK_PBP) $@ PARAM.SFO \
		$(PSP_EBOOT_ICON) \
		$(PSP_EBOOT_PIC1) \
		NULL NULL NULL NULL NULL NULL

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET).elf PARAM.SFO EBOOT.PBP

# Clean everything including build output
distclean: clean

.PHONY: all clean distclean
