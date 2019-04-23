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
	if (bind(managerfd, (struct sockaddr *)&un, size) < 0){
		perror("bind failed");
		exit(1);
	}
	if (listen(managerfd, listenQ) < 0){
		perror("listen failed");
		exit(1);
	}
	cout << "GTStoreManager::init() Done\n";
}

int GTStoreManager::manage_client_request(Message &m, int fd){
	printf("Manager connected to some client\n");

	printf("Got request from client\n");
	m.type |= ERROR_MASK;
	m.node_id = -1;
	m.length = 0;
	m.send(fd);
	printf("Sent contact for client\n");
	return 0;
}

int GTStoreManager::manage_node_request(Message &m, int fd){
	// Manage entrance and exit status of nodes
	printf("Manager connected to some node\n");

	printf("");
	int num_vnodes;
	sscanf(m.data, "%d", &num_vnodes);
	int sid = -1;
	vector<VirtualNodeID> vvid = {};
	node_table.add_storage_node(num_vnodes, sid, vvid);

	// send back virtual node ids
	m.type = MSG_NODE_REPLY;
	m.node_id = sid;
	if (m.data) delete[] m.data;
	m.length = num_vnodes * 64;
	m.data = new char[m.length];
	for (int i=0; i<num_vnodes; i++) {
		sprintf(m.data + 64*i, "%d", vvid[i]);
	}
	m.send(fd, m.data);
	if (m.data) delete[] m.data;
	printf ("Add a new node\n");

	return 0;
}

void GTStoreManager::exec(){
	int connfd;
	while (1){
		connfd = accept(managerfd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m;
		m.recv(connfd);
		m.print();
		if (m.type & CLIENT_MASK) {
			manage_client_request(m, connfd);
		} else if (m.type & NODE_MASK) {
			manage_node_request(m, connfd);
		}
		close(connfd);
	}
}

int main(int argc, char **argv) {

	GTStoreManager manager;
	manager.init();
	manager.exec();
}
