PREX = arm-linux-gnueabihf-
CC = $(PREX)gcc
STRIP = $(PREX)strip
CFLAG =

LIBS = -lm -lpthread
C_SRCS = $(wildcard *.c)
APP = v4l2grab

$(APP) : $(C_SRCS)
	$(CC) -g -O2 -o $@ $(C_SRCS) $(INCLUDES) $(LDFLAGS) $(CFLAG) $(LIBS) -D_GNU_SOURCE
	@$(STRIP) $(APP)
clean:
	@rm -fr $(APP)
