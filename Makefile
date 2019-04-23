CPPFLAGS = -Wall
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf

TESTS = manager storage test_socket
OBJS =  client.o

all: $(TESTS)

client.o:
	$(CC) $(CFLAGS) -c client.cpp -o client.o

manager: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) manager.cpp -o manager

storage: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) storage.cpp -o storage

test_app: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) test_app.cpp -o test_app

clean:
	$(RM) *.o $(TESTS) *.socket*

test_socket: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) test_socket.cpp -o test_socket
