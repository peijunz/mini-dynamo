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
	VirtualNodeID vid;
	StorageNodeID sid;
	for (int i=0; i<num_vnodes; i++) {
		sscanf(m.data + 16 + i * 32, "%d", &vid);
		sscanf(m.data + 32 + i * 32, "%d", &sid);
		node_table.add_virtual_node(vid);
		node_table.storage_nodes.insert({vid, sid});
		printf ("{%d, %d}\t", vid, sid);
	}
	printf("\n");

	printf ("Successfully add to manager!  Node ID = %d\n", id);
	close(fd);


	int size;
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	unlink(storage_node_addr(id).data());
	strcpy(un.sun_path, storage_node_addr(id).data());
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
			// Because client does not listen, we do not
			// close client until we got a reply
			process_client_request(m, connfd);
		} else{
			if (m.type & NODE_MASK) {
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
}

bool GTStoreStorage::process_client_request(Message& m, int fd) {
	// Find coordinator and forward request
	int offset = strnlen(m.data, m.length)+1;
	char *val = m.data + offset;
	StorageNodeID sid = find_coordinator(m.data);
	if (sid == id){
		// Do not forward, reply and then close
		process_node_request(m, fd);
		// m.data
		// m.send(fd)
		// close(fd);
	}
	else{
		// Forward message
		forward_tasks[m.client_id] = fd;
		m.type = MSG_COORDINATOR_REQUEST;
		int fwdfd = openfd(storage_node_addr(sid).data());
		m.send(fwdfd, m.data);
		close(fwdfd);
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
	working_tasks.insert({m.client_id, {0, Data()}});

	// itself
	pref_list.erase(pref_list.begin());
	if (m.type & WRITE_MASK) {
		// write
		write_local(key, data);
	} else {
		// read
		read_local(key, data);
	}
	working_tasks[m.client_id].first ++;
	working_tasks[m.client_id].second = data;

	// ask other nodes to complete their work
	for (auto& pref : pref_list) {
		m.type = MSG_COORDINATOR_REQUEST | (m.type & WRITE_MASK);
		string coordinator_addr = node_addr + "_" + to_string(pref.second);
		int nodefd = openfd(coordinator_addr.data());
		m.send(nodefd, m.data);
		close(nodefd);
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
		m.type = MSG_COORDINATOR_REPLY | (m.type & WRITE_MASK);
		m.send(fd);
	} 
	else {
		// read
		read_local(key, data);
		m.type = MSG_COORDINATOR_REPLY | (m.type & WRITE_MASK);
		m.set_key_data(key, data);
		m.send(fd, m.data);
	}
	return true;
}

bool GTStoreStorage::process_coordinator_reply(Message& m, int fd) {
	if (working_tasks.count(m.client_id) == 0) {
		// task is already completed. Ignore redundant result
		return false;
	}

	string key; 
	Data data;
	m.get_key_data(key, data);
	working_tasks[m.client_id].first ++;

	if ((m.type | WRITE_MASK) ||
		data.version >= working_tasks[m.client_id].second.version)
	{
		// new version, update result
		working_tasks[m.client_id].second = data;
	}

	if (working_tasks[m.client_id].first >= 
		((m.type & WRITE_MASK) ? CONFIG_W : CONFIG_R))
	{
		// task completed. send back to transferrer
		m.type = MSG_NODE_REPLY | (m.type & WRITE_MASK);
		m.set_key_data(key, working_tasks[m.client_id].second);
		string transferrer_addr = node_addr + "_" + to_string(m.node_id);
		int nodefd = openfd(transferrer_addr.data());
		m.send(nodefd, m.data);
		close(nodefd);
	}

	return false;
}

bool GTStoreStorage::process_manager_reply(Message& m, int fd) {
	
	// a new node with id==m.node_id joins
	int num_vnodes;
	sscanf(m.data, "%d", &num_vnodes);
	vector<VirtualNodeID> vvid(num_vnodes);
	printf ("%d join, %d vnodes\n", m.node_id, num_vnodes);
	printf ("%.*s\n", m.length, m.data);
	for (int i=0; i<num_vnodes; i++) {
		sscanf(m.data + 16*(i+1), "%d", &vvid[i]);
	}
	node_table.add_storage_node(num_vnodes, m.node_id, vvid);

	printf ("Successfully add to manager!  Node ID = %d\n", id);
	close(fd);

	return true;
}
int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	storage.exec();
}