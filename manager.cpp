#include "gtstore.hpp"

void GTStoreManager::init() {
	int size;
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, manager_addr);
	if ((managerfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket failed");
		exit(1);
	}
	size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (bind(managerfd, (struct sockaddr *)&un, size) < 0)
	if ((managerfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("bind failed");
		exit(1);
	}
	if (listen(managerfd, listenQ) < 0){
		perror("listen failed");
		exit(1);
	}
	cout << "GTStoreManager::init() Done\n";
}

void GTStoreManager::exec(){
	int connfd;
	while (1){
		connfd = accept(managerfd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m(connfd);
		printf("Manager connected to some client\n");
		close(connfd);
	}
}

int main(int argc, char **argv) {

	GTStoreManager manager;
	manager.init();
	
}