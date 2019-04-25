#include <signal.h>
#include "gtstore.hpp"

vector<int> children;
char *args[] = { 0 }; /* each element represents a command line argument */
char *env[] = { 0 }; /* leave the environment list null */

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        for(int i=1; i<children.size(); i++){
            int pid = children[i];
            kill(pid, SIGTERM);
            waitpid(pid, NULL, NULL);
            //sleep(1);
        }
        kill(children[0], SIGTERM);
    	exit(1);
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

int create_storage_node(int i) {
    int pid;
    if ((pid = Fork()) == 0) {
        printf("========================\n");
        printf("=== Starting Storage Node %d...\n", i);
        execve("./storage", args, env);
        return 0;
    }
    // usleep(300000);
    printf("========================\n");
    children.push_back(pid);
    return pid;
}


int main() {
    int manager_id, pid;
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
    for (int i=0; i<2*CONFIG_N; i++){
        create_storage_node(i);
    }
    sleep(1);
    for (int i=0; i<1; i++){
        if ((pid = Fork()) == 0) {
            printf("========================\n");
            printf("=== Running Client %d...\n", i);
            GTStoreClient client;
            client.init(i);
            client.put("test_key_"+to_string(i), "test_val_"+to_string(i));
            client.get("test_key_"+to_string(i));
            sleep(5);
            client.get("test_key_"+to_string(i));
            return 0;
        }
        children.push_back(pid);
    }

    sleep(1);
    for (int i=0; i<2*CONFIG_N; i++) {
        int& pid = children[i+1];
        kill(pid, SIGTERM);
        waitpid(pid, NULL, NULL);
        pid = create_storage_node(i+2*CONFIG_N);
        usleep(3e5);
    }

    if(SIG_ERR == signal(SIGINT, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
        exit(1);
    }

    if(SIG_ERR == signal(SIGTERM, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
        exit(1);
    }
    sleep(10);
    _sig_handler(SIGINT);
    return 0;
}
