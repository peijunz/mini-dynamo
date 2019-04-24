#include "gtstore.hpp"


void GTStoreManager::init() {
	cout << ">>> GTStoreManager::init()\n";
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
	cout << "<<< GTStoreManager::init() Done\n";
}

int GTStoreManager::manage_client_request(Message &m, int fd){
	printf(">>> %s: Entering\n", __func__);
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
	printf("<<< %s: Exiting\n", __func__);
	return 0;
}

int GTStoreManager::manage_node_request(Message &m, int fd){
	// Manage entrance and exit status of nodes
	int num_new_vnodes;
	sscanf(m.data, "%d", &num_new_vnodes);
	printf(">>> %s: Entering with %d requested vnodes\n", __func__, num_new_vnodes);
	int sid = -1;
	vector<VirtualNodeID> vvid = {};
	node_table.add_storage_node(num_new_vnodes, sid, vvid);
	node_table.nodes.insert(sid);

	m.type = MSG_MANAGE_REPLY;
	m.node_id = sid;
	storage_nodes.insert(sid);

	// copy to new node
	if (m.data) delete[] m.data;
	int num_vnodes = node_table.storage_nodes.size();
	m.data = new char[16 + 32 * num_vnodes];
	m.length = 1+sprintf(m.data, "%d", num_vnodes);
	for (auto& p : node_table.storage_nodes) {
		m.length += 1+sprintf(m.data + m.length, "%d %d", p.first, p.second);
	}
	m.owner = __func__;
	m.send(fd, m.data);
	m.recv(fd);
	close(fd);
	
	// compute donate information
	unordered_map<StorageNodeID, vector<pair<VirtualNodeID, VirtualNodeID>> >	donate_info;
	if (node_table.nodes.size() > CONFIG_N) {
		for (VirtualNodeID vid : vvid) {
			// head: k previous
			auto& vnodes = node_table.virtual_nodes;
			auto& snodes = node_table.storage_nodes;
			auto new_node = vnodes.find(vid);
			auto it = new_node;
			unordered_map<VirtualNodeID, int> count;
			while (count.size() < CONFIG_N) {
				count[snodes[it->second]] ++;
				if (it == vnodes.begin())
					it = prev(vnodes.end());
				else it --;
				if (snodes[it->second] == snodes[new_node->second] || it == new_node) 
					break;			
			}

			auto jt = new_node;
			while (count.size() < CONFIG_N+1) {
				jt ++;
				if (jt == vnodes.end())
					jt = vnodes.begin();
				count[snodes[jt->second]] ++;
			}
			
			while (it != new_node) {
				VirtualNodeID vid_start = it->second;

				it++;
				if (it == vnodes.end())
					it = vnodes.begin();
				if (--count[snodes[it->second]] == 0)
					count.erase(snodes[it->second]);

				VirtualNodeID vid_end = it->second;
				StorageNodeID sid_donate = snodes[jt->second];

				donate_info[sid_donate].push_back({vid_start, vid_end});

				while (count.size() < CONFIG_N+1) {
					jt ++;
					if (jt == vnodes.end())
						jt = vnodes.begin();
					count[snodes[jt->second]] ++;
				}
			}
		}
	}

	// broadcast to old nodes
	printf("\t%s: Broadcast to storage nodes %s\n", __func__);

	for (StorageNodeID nodeid = 0; nodeid < node_table.num_storage_nodes; nodeid ++) {

		if (m.data) delete[] m.data;
		m.type = MSG_MANAGE_REPLY;
		m.data = new char[(1 + num_new_vnodes) * 16];
		m.length = 1+sprintf(m.data, "%d", num_new_vnodes);
		for (int i=0; i<num_new_vnodes; i++) {
			m.length += 1+sprintf(m.data + m.length, "%d", vvid[i]);
		}

		if (nodeid == sid) continue;
		fd = openfd(storage_node_addr(nodeid).data());
		if (fd<0){
			perror("wrong fd\n");
		}
		m.send(fd, m.data);
		m.set_intervals(donate_info[nodeid]);
		m.send(fd, m.data);
		close(fd);
	}

	if (m.data) delete[] m.data;
	printf("<<< %s: Exiting\n", __func__);



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
