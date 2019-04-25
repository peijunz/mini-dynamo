#include <signal.h>
#include "gtstore.hpp"

vector<int> children;

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        for(int pid:children){
            kill(pid, SIGTERM);
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
    char *args[] = { 0 }; /* each element represents a command line argument */
    char *env[] = { 0 }; /* leave the environment list null */
    if ((manager_id = Fork()) == 0) {
        unlink(manager_addr);
        printf("========================\n");
        printf("=== Starting Manager with (N=%d, R=%d, W=%d, V=%d)\n",\
                CONFIG_N, CONFIG_R, CONFIG_W, CONFIG_V);
        execve("./manager", args, env);
        return 0;
    }
    children.push_back(manager_id);
    sleep(1);
    for (int i=0; i<6; i++){
        if ((pid = Fork()) == 0) {
            printf("========================\n");
            printf("=== Starting Storage Node %d...\n", i);
            execve("./storage", args, env);
            return 0;
        }
        usleep(300000);
        printf("========================\n");
        children.push_back(pid);
    }
    sleep(1);
    for (int i=0; i<0; i++){
        if ((pid = Fork()) == 0) {
            printf("========================\n");
            printf("=== Running Client %d...\n", i);
            GTStoreClient client;
            client.init(i);
            client.put("test_key_"+to_string(i), "test_val_"+to_string(i));
            client.get("test_key_"+to_string(i));
            return 0;
        }
        children.push_back(pid);
    }

    if(SIG_ERR == signal(SIGINT, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
        exit(1);
    }

    if(SIG_ERR == signal(SIGTERM, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
        exit(1);
    }
    sleep(3);
    _sig_handler(SIGINT);
    return 0;
}
