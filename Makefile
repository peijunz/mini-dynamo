CFLAGS  =
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf

TESTS = test_app manager storage test_socket
SRC = test_app.cpp client.cpp

all: $(TESTS)

manager: manager.cpp
	$(CC) -Wall manager.cpp -o manager

storage: storage.cpp
	$(CC) -Wall storage.cpp -o storage

test_app : 
	$(CC) -Wall $(SRC) -o test_app

clean:
	$(RM) *.o $(TESTS)

test_socket:
	$(CC) test_socket.cpp -o test_socket
