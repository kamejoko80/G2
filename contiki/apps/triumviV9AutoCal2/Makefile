DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = triumviV9

TARGETDIRS += ../../platform

all: $(CONTIKI_PROJECT)

UIP_CONF_IPV6=0

CONTIKI = ../../../../contiki
include $(CONTIKI)/Makefile.include

CFLAGS += -DCC2538_RF_CONF_CHANNEL=11
CFLAGS += -DTHREEPHASE_ID=$(threephaseID)
#CFLAGS += -DCALIBRATE_FILE_NAME=\"$(calFile)\"
# Syntax is following
# make threephaseID=1
