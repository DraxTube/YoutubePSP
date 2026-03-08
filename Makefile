TARGET = PSPTube
OBJS   = src/main.o src/graphics.o src/network.o src/youtube.o \
         src/config.o src/ui.o src/video.o

PSPSDK = $(shell psp-config --pspsdk-path)

CFLAGS = -O2 -Wall -G0 \
         -I$(PSPSDK)/include \
         -I$(PSPSDK)/../include \
         -Isrc \
         -D_PSP_FW_VERSION=600

LIBS   = -lpsphttp -lpspssl \
         -lpspnet_apctl -lpspnet_inet -lpspnet \
         -lpsputility \
         -lpspmpeg \
         -lpspaudio \
         -lpspgu -lpspge -lpspdisplay \
         -lpspctrl -lpspwlan -lpsppower \
         -lpspsdk -lc -lm \
         -lpspdebug

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = PSPTube
PSP_EBOOT_ICON  = assets/ICON0.PNG
PSP_EBOOT_PIC1  = assets/PIC1.PNG

include $(PSPSDK)/lib/build.mak
