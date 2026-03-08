TARGET = PSPTube
OBJS   = src/main.o src/graphics.o src/network.o src/youtube.o \
         src/config.o src/ui.o src/video.o

# PSP SDK paths (set by GitHub Actions toolchain)
PSPSDK  = $(shell psp-config --pspsdk-path)
PSPDIR  = $(shell psp-config --psp-prefix)

# Include / library paths
INCDIR  = $(PSPSDK)/include
LIBDIR  = $(PSPSDK)/lib
CFLAGS  = -O2 -Wall -G0 -I$(INCDIR) -Isrc

# PSP-specific libs
LIBS    = -lpsphttp -lpspssl -lpspnet_apctl -lpspnet_inet -lpspnet \
          -lpsputility -lpspmpeg -lpspatrac3plus \
          -lpspaudio -lpspgu -lpspge -lpspdisplay \
          -lpspctrl -lpsppower -lpspsdk -lc -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSPTube
PSP_EBOOT_ICON  = assets/ICON0.PNG
PSP_EBOOT_PIC1  = assets/PIC1.PNG

include $(PSPSDK)/lib/build.mak
