#include "gtstore.hpp"


void GTStoreStorage::init(int num_vnodes) {
	// TODO: Contact Manager to get global data
	int size;
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	string storage_node_addr = node_addr + "_" + to_string(id);
	strcpy(un.sun_path, storage_node_addr.data());
	if ((nodefd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket failed");
		exit(1);
	}
	size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (bind(nodefd, (struct sockaddr *)&un, size) < 0)
	if ((nodefd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("bind failed");
		exit(1);
	}
	if (listen(nodefd, listenQ) < 0){
		perror("listen failed");
		exit(1);
	}

	// add to manager, get node id

    int fd = openfd(manager_addr);
    if (fd < 0){
        printf("error in nodefd\n");
        exit(-1);
    }
    Message m(MSG_NODE_REQUEST, -1, -1, 0);
	m.length = 64;
	m.data = new char[m.length];
	sprintf(m.data, "%d", num_vnodes);
    m.send(fd);
    m.recv(fd);

	if (m.type != MSG_NODE_REPLY) {
		perror ("manager returns with unmatched message type!\n");
		exit(-1);
	}
	id = m.node_id;
	vector<VirtualNodeID> vvid(num_vnodes);
	for (int i=0; i<num_vnodes; i++) {
		sscanf(m.data + 64*i, "%d", &vvid[i]);
	}
	node_table.add_storage_node(num_vnodes, id, vvid);
	printf ("Successfully add to manager!\n");

	close(fd);
	if (m.type & ERROR_MASK){
		printf("No available node\n");
		//exit(1);
	}

	cout << "Inside GTStoreStorage::init()\n";
}

bool GTStoreStorage::read_local(string key, Data& data, VirtualNodeID v_id) {
	if (this->data.count(v_id) == 0 ||
		this->data[v_id].count(key) == 0)
	{
		data.version = 0;
		return false;
	}
	else
	{
		data = this->data[v_id][key];
		return true;
	}
	
}
bool GTStoreStorage::write_local(string key, Data data, VirtualNodeID v_id) {
	if (this->data.count(v_id) == 0 ||
		this->data[v_id].count(key) == 0 ||
		this->data[v_id][key].version < data.version)
	{
		this->data[v_id][key] = data;
	}
	return true;
}


StorageNodeID GTStoreStorage::find_coordinator(string key) {
	return node_table.get_preference_list(key, 1)[0].second;
}



bool GTStoreStorage::read_remote(string key, Data& data, StorageNodeID, VirtualNodeID) {
	return false;
}

void GTStoreStorage::exec() {

	int connfd;
	while (1){
		connfd = accept(nodefd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m;
		m.recv(connfd);
		printf("Manager connected to some client\n");
		if (m.type & CLIENT_MASK) {
			process_client_request(m, connfd);
		} else if (m.type & NODE_MASK) {
			process_node_request(m, connfd);
		} else if (m.type & COOR_MASK) {
			process_coordinator_request(m, connfd);
		}
		close(connfd);
	}
}

bool GTStoreStorage::process_client_request(Message& m, int fd) {
	// Find coordinator and forward request
	int offset = strnlen(m.data, m.length)+1;
	char *val = m.data + offset;
	StorageNodeID sid = find_coordinator(m.data);
	if (sid == id){
		// Do not forward
	}
	else{
		// Forward message
	}
	return false;
}
bool GTStoreStorage::process_node_request(Message& m, int fd) {
	// collect R/W from pref list
	return false;
}
bool GTStoreStorage::process_coordinator_request(Message& m, int fd) {
	// Do as coordinator asked to do
	return false;
}

int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	//storage.exec();
	
}