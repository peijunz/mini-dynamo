CPPFLAGS = -Wall
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf

TESTS = test_app manager storage test_socket
CPP_FILES = client.cpp storage.cpp manager.cpp
OBJS =  client.o

all: $(TESTS)

client.o:
	$(CC) $(CFLAGS) -c client.cpp -o client.o

manager:
	$(CC) $(CFLAGS) client.o manager.cpp -o manager

storage: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) storage.cpp -o storage

test_app: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) test_app.cpp -o test_app

clean:
	$(RM) *.o $(TESTS)

test_socket: $(OBJS)
	$(CC) $(CFLAGS) $(CPP_FILES) test_socket.cpp -o test_socket
