#include "gtstore.hpp"

int main() {
    Message msg(MSG_CLIENT_REQUEST, "client_0", "node_0", 10);
    msg.send_to(manager_addr, "asdfghjkl;");

    return 0;
}