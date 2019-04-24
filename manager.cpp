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
	if (!storage_nodes.size()){
		m.type |= ERROR_MASK;
		m.node_id = 0;
	}
	else{
		auto it = storage_nodes.upper_bound(cur_contact);
		if (it == storage_nodes.end())
			it = storage_nodes.begin();
		cur_contact = *it;
		m.node_id = cur_contact;
	}
	m.length = 0;
		m.owner = __func__;
	m.send(fd);
	printf("Sent contact for client\n");
	return 0;
}

int GTStoreManager::manage_node_request(Message &m, int fd){
	// Manage entrance and exit status of nodes
	printf("Manager connected to some node\n");

	printf("");
	int num_new_vnodes;
	sscanf(m.data, "%d", &num_new_vnodes);
	int sid = -1;
	vector<VirtualNodeID> vvid = {};
	node_table.add_storage_node(num_new_vnodes, sid, vvid);

	m.type = MSG_MANAGE_REPLY;
	m.node_id = sid;
	storage_nodes.insert(sid);

	// copy to new node
	if (m.data) delete[] m.data;
	int num_vnodes = node_table.storage_nodes.size();
	m.length = 16 + 32 * num_vnodes;
	m.data = new char[m.length];
	sprintf(m.data, "%d", num_vnodes);
	int i = 0;
	for (auto& p : node_table.storage_nodes) {
		sprintf(m.data + 16 + i * 32, "%d %d", p.first, p.second);
		i ++;
	}
		m.owner = __func__;
	m.send(fd, m.data);
	m.recv(fd);
	close(fd);

	// broadcast to old nodes
	if (m.data) delete[] m.data;
	m.type = MSG_MANAGE_REPLY;
	m.length = (1 + num_new_vnodes) * 16;
	m.data = new char[m.length];
	sprintf(m.data, "%d", num_new_vnodes);
	for (int i=0; i<num_new_vnodes; i++) {
		sprintf(m.data + 16*(i+1), "%d", vvid[i]);
	}
	for (StorageNodeID nodeid = 0; nodeid < node_table.num_storage_nodes; nodeid ++) {
		if (nodeid == sid) continue;
		fd = openfd(storage_node_addr(nodeid).data());
		if (fd<0){
			perror("wrong fd\n");
		}
		m.send(fd, m.data);
		close(fd);
	}

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
		} else if (m.type & FORWARD_MASK) {
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
