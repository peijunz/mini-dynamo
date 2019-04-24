#include <signal.h>
#include "gtstore.hpp"

vector<int> children;

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        for(int pid:children){
            kill(pid, SIGINT);
        }
    }
}

int Fork(){
    int pid;
    if ((pid = fork()) == -1){
        perror("Fork failed");
        exit(1);
    }
    return pid;
}

int main() {
    int manager_id, pid;
    if ((manager_id = Fork()) == 0) {
        unlink(manager_addr);
        execve("./manager", NULL, NULL);
        return 0; 
    }
    children.push_back(manager_id);
    sleep(1);
    for (int i=0; i<1; i++){
        if ((pid = Fork()) == 0) {
            execve("./storage", NULL, NULL);
            return 0;
        }
        children.push_back(pid);
    }
    sleep(1);
    fprintf(stderr, "===============================\n");
    for (int i=0; i<1; i++){
        if ((pid = Fork()) == 0) {
            char key[32], val[32];
            GTStoreClient client;
            client.init(i);
            client.put("test_key_"+to_string(i), "test_val_"+to_string(i));
            client.get("test_key_"+to_string(i));
            return 0;
        }
        children.push_back(pid);
    }
    int status;

    if(SIG_ERR == signal(SIGINT, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
        exit(1);
    }

    if(SIG_ERR == signal(SIGTERM, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
        exit(1);
    }

    waitpid(manager_id, &status, 0);
    _sig_handler(SIGINT);
    return 0;
}