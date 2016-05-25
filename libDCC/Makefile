BASE_DIR =..
LIB_INC  = $(BASE_DIR)/include

CFLAGS += -DLINUX -g -Wall -I$(LIB_INC)


TARGET = libDCC.a
all: $(TARGET)

$(TARGET): dcc.o md5.o
	$(AR) $(ARFLAGS) $@ $? 
dcc.o: dcc.c
md5.o: md5.c

clean:
	$(RM) -f $(TARGET) dcc.o md5.o


install:
  
