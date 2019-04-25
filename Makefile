CPPFLAGS = -Wall
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf

TESTS = manager storage test_app
OBJS =  client.o

all: $(TESTS)

client.o:
	$(CC) $(CPPFLAGS) -c client.cpp -o client.o

manager: $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) manager.cpp -o manager

storage: $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) storage.cpp -o storage

test_app: $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) test_app.cpp -o test_app

clean:
	$(RM) *.o $(TESTS) *.socket*
