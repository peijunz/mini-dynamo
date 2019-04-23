#include "gtstore.hpp"



vector<pair<VirtualNodeID, StorageNodeID>> NodeTable::get_preference_list(string key, int size) {
	size_t hash_key = consistent_hash(key);
	auto it = virtual_nodes.upper_bound(hash_key);
	vector<pair<VirtualNodeID, StorageNodeID>>	pref_list;
	for (int i=0; i<size; i++) {
		if (it == virtual_nodes.end())
			it = virtual_nodes.begin();
		pref_list.push_back({
			it->second, 				// virtual node id
			storage_nodes[it->first]	// storage node id
		});
	}
	return pref_list;
}






void GTStoreStorage::init() {
	
	int size;
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	string storage_node_addr = node_addr + "_" + id;
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
		Message m(connfd);
		printf("Manager connected to some client\n");
		if (m.type | CLIENT_MASK) {
			process_client_request(m);
		} else if (m.type | NODE_MASK) {
			process_node_request(m);
		} else if (m.type | COOR_MASK) {
			process_coordinator_request(m);
		}
		close(connfd);
	}
}

bool GTStoreStorage::process_client_request(Message& msg) {

	return false;
}
bool GTStoreStorage::process_node_request(Message& msg) {
	return false;
}
bool GTStoreStorage::process_coordinator_request(Message& msg) {
	return false;
}

int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	
}