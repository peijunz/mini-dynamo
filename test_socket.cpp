#include "gtstore.hpp"

int main() {
    Message msg(MSG_CLIENT_REQUEST, "client_0", "NULL", 10);
    int fd = open_clientfd(manager_addr);
    if (fd < 0){
        printf("error in clientfd");
        exit(-1);
    }
    msg.send(fd, "asdfghjkl;");

    return 0;
}