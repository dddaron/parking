#common makefile header

DIR_INC = ../include
DIR_BIN = ../bin
DIR_LIB = ../libs

TARGET	= s_Server
BIN_TARGET = $(DIR_INC)/../$(TARGET)

CROSS_COMPILE = 
CFLAGS = -g -Wall -I$(DIR_INC)

#ifdef LINUX64
LDFLAGS := -L$(DIR_LIB)/x64
#else
#LDFLAGS := -L$(DIR_LIB)/x86 
#endif
LDFLAGS += -lmsc -lrt -ldl -lpthread -lstdc++ -lmysqlclient

OBJECTS := $(patsubst %.c,%.o,$(wildcard *.c))

$(BIN_TARGET) : $(OBJECTS)
	$(CROSS_COMPILE)gcc $(CFLAGS) $^ -o $@ $(LDFLAGS)
	#export LD_LIBRARY_PATH=/home/project/voice/libs/x64/
	export LD_LIBRARY_PATH=$(pwd)/../libs/x64/
%.o : %.c
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $< -o $@
clean:
	@rm -f *.o $(BIN_TARGET)

.PHONY:clean

#common makefile foot
