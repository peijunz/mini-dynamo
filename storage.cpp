#include "gtstore.hpp"


void GTStoreStorage::init(int num_vnodes) {
	// TODO: Contact Manager to get global data


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
    m.send(fd, m.data);

    m.recv(fd);
	if (m.type & ERROR_MASK){
		printf("No available node\n");
		exit(1);
	}

	if (m.type != MSG_MANAGER_REPLY) {
		perror ("manager returns with unmatched message type!\n");
		exit(-1);
	}
	id = m.node_id;
	sscanf(m.data, "%d", &num_vnodes);
	vector<VirtualNodeID> vvid(num_vnodes);
	for (int i=0; i<num_vnodes; i++) {
		sscanf(m.data + 64*(i+1), "%d", &vvid[i]);
	}
	node_table.add_storage_node(num_vnodes, id, vvid);

	printf ("Successfully add to manager!  Node ID = %d\n", id);
	close(fd);


	int size;
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	string storage_node_addr = node_addr + "_" + to_string(id);
	unlink(storage_node_addr.data());
	strcpy(un.sun_path, storage_node_addr.data());
	if ((nodefd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket failed");
		exit(1);
	}
	size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (bind(nodefd, (struct sockaddr *)&un, size) < 0) {
		perror("bind failed");
		exit(1);
	}
	if (listen(nodefd, listenQ) < 0){
		perror("listen failed");
		exit(1);
	}
	cout << "Inside GTStoreStorage::init()\n";
}

bool GTStoreStorage::read_local(string key, Data& data) {
	VirtualNodeID v_id = node_table.find_virtual_node(key);
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
bool GTStoreStorage::write_local(string key, Data data) {
	VirtualNodeID v_id = node_table.find_virtual_node(key);
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
			if (m.type & REPLY_MASK)
				process_node_reply(m, connfd);
			else
				process_node_request(m, connfd);
		} else if (m.type & COOR_MASK) {
			if (m.type & REPLY_MASK)
				process_node_reply(m, connfd);
			else
				process_coordinator_request(m, connfd);
		} else if (m.type & MANAGER_MASK) {
			if (m.type & REPLY_MASK)
				process_manager_reply(m, connfd);
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
	// do as a coordinator. Collect R/W from pref list
	string key;
	Data data;
	m.get_key_data(key, data);
	auto pref_list = node_table.get_preference_list(key, CONFIG_N);

	// initialize task
	current_task[m.client_id] = 0;

	// itself
	pref_list.erase(pref_list.begin());
	if (m.type & WRITE_MASK) {
		// write
		write_local(key, data);
	} else {
		// read
		read_local(key, data);
	}
	current_task[m.client_id] += 1;

	// ask other nodes to complete their work
	for (auto& pref : pref_list) {
		m.type = MSG_COORDINATOR_REQUEST | (m.type & WRITE_MASK);
		m.node_id = pref.second;
		
		
	}

	return false;
}

bool GTStoreStorage::process_node_reply(Message& msg, int fd) {
	return false;
}
bool GTStoreStorage::process_coordinator_request(Message& m, int fd) {
	// Do as coordinator asked to do

	string key;
	Data data;
	m.get_key_data(key, data);
	if (m.type & WRITE_MASK) {
		// write
		write_local(key, data);
		m.type = MSG_COORDINATOR_REPLY;
		m.send(fd);
	} 
	else {
		// read
		read_local(key, data);
		m.type = MSG_COORDINATOR_REPLY;
		m.set_key_data(key, data);
		m.send(fd, m.data);
	}
	return true;
}

bool GTStoreStorage::process_coordinator_reply(Message& m, int fd) {
	return false;
}

bool GTStoreStorage::process_manager_reply(Message& m, int fd) {
	
	// a new node with id==m.node_id joins
	int num_vnodes;
	sscanf(m.data, "%d", &num_vnodes);
	vector<VirtualNodeID> vvid(num_vnodes);
	for (int i=0; i<num_vnodes; i++) {
		sscanf(m.data + 64*(i+1), "%d", &vvid[i]);
	}
	node_table.add_storage_node(num_vnodes, id, vvid);

	printf ("Successfully add to manager!  Node ID = %d\n", id);
	close(fd);

	return true;
}
int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	storage.exec();
}