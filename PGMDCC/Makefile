OBJS =  log.o general.o db_manage.o program_dcc.o
EXECUTABLE = DCC.exec
CC = g++

CFLAGS += -I ../include -g -Wall
CFLAGS += -I /usr/include/mysql -g -Wall
LDFLAGS += -L../libDCC -lDCC
LDFLAGS += -L/usr/lib64/mysql -lmysqlclient -lncurses -g -Wall
$(EXECUTABLE):$(OBJS)
	$(CC) $+ $(LDFLAGS) -o $(EXECUTABLE)

clean:
	$(RM) -rf $(OBJS) $(EXECUTABLE) *~ core *.gdb
