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
	if (!node_table.nodes.size()){
		m.type |= ERROR_MASK;
		m.node_id = -1;
	}
	else{
		auto it = node_table.nodes.upper_bound(cur_contact);
		if (it == node_table.nodes.end())
			it = node_table.nodes.begin();
		cur_contact = it->first;
		m.node_id = cur_contact;
	}
	m.length = 0;
	m.owner = __func__;
	m.send(fd);
	printf("<<< %s: Exiting\n", __func__);
	return 0;
}

unordered_map<StorageNodeID, vector<pair<VirtualNodeID, VirtualNodeID>>>
GTStoreManager::donate_information(vector<VirtualNodeID>&vvid){

	// compute donate information
	unordered_map<StorageNodeID, vector<pair<VirtualNodeID, VirtualNodeID>> >	donate_info;
	node_table.print_ring();
	for (VirtualNodeID vid : vvid) {
		printf("\tcomputing.......%u\n", vid);
		// head: k previous
		auto& vnodes = node_table.virtual_nodes;
		auto& snodes = node_table.storage_nodes;
		auto new_node = vnodes.find(node_table.consistent_hash(virtual_node_addr(vid)));
		auto it = new_node;
		unordered_map<VirtualNodeID, int> count;
		printf(">>>>>>>>>>>>>> %d\n", it->second);
		while(1) {
			count[snodes[it->second]] ++;
			if (count.size() == CONFIG_N+1){
				break;
			}
			if (snodes[it->second] == snodes[new_node->second] && it != new_node) 
			//if (it == new_node)
				break;			
			if (it == vnodes.begin())
				it = prev(vnodes.end());
			else it --;
		printf(">>>>>>>>>>>>>> %d\n", it->second);
		}
			// it++;
			// if (it == vnodes.end())
			// 	it = vnodes.begin();
		auto jt = new_node;
		// while (count.size() < CONFIG_N+1) {
		// 	jt ++;
		// 	if (jt == vnodes.end())
		// 		jt = vnodes.begin();
		// 	count[snodes[jt->second]] ++;
		// }
		
		// fprintf(stderr, "\t\tset head and tail: %ld possible donators\n", count.size());

		while (it != new_node) {

			// Invariant: it~jt has N+1 nodes
			// auto kt = it;
			// while (kt != jt) {
			// 	fprintf(stderr, "\t{%d,%d}", kt->second, snodes[kt->second]);
			// 	kt ++;
			// 	if (kt == vnodes.end()) kt = vnodes.begin();
			// }
			// fprintf(stderr, "\t{%d,%d}", kt->second, snodes[kt->second]);
			// fprintf(stderr, "\n");
			

			//
			VirtualNodeID vid_start = it->second;

			if (--count[snodes[it->second]] == 0)
				count.erase(snodes[it->second]);
			it++;
			if (it == vnodes.end())
				it = vnodes.begin();

			while (count.size() < CONFIG_N+1) {
				jt ++;
				if (jt == vnodes.end())
					jt = vnodes.begin();
				count[snodes[jt->second]] ++;
			}

			VirtualNodeID vid_end = it->second;
			StorageNodeID sid_donate = snodes[jt->second];

			donate_info[sid_donate].push_back({vid_start, vid_end});
			printf("\t\t||||----Donate: %d, [%d, %d)----||||\n", sid_donate, vid_start, vid_end);

		}
	}
	// exit(1);

	fprintf(stderr, "\t\tfind %ld donators\n", donate_info.size());
	return donate_info;
}

int GTStoreManager::manage_node_request_leave(Message &m, int fd){
	fprintf(stderr, ">>> GTStoreManager: %s\n", __func__);
	int sid = m.node_id;
	printf("#nodes= %ld\n", node_table.nodes.size());
	node_table.remove_storage_node(sid);
	printf("#nodes= %ld\n", node_table.nodes.size());
	m.owner = __func__;
	m.type = MANAGE_MASK|LEAVE_MASK;

	for (auto& x: node_table.nodes) {
		printf("Sending close to %d\n", x.first);
		StorageNodeID nodeid = x.first;

		assert (nodeid != sid);
		int fd2 = openfd(storage_node_addr(nodeid).data());
		if (fd2<0){
			perror("ERROR: wrong fd in manage node req leave\n");
			exit(1);
		}
		m.send(fd2);
		m.print("Manager send leaving message:---------------\n");
		close(fd2);
	}
	fprintf(stderr, "<<< GTStoreManager: %s\n", __func__);
	m.send(fd);
	return 0;
}

int GTStoreManager::manage_node_request(Message &m, int fd){
	// Manage entrance of nodes
	int num_new_vnodes;
	sscanf(m.data, "%d", &num_new_vnodes);
	printf(">>> %s: Entering with %d requested vnodes\n", __func__, num_new_vnodes);
	int sid = -1;
	vector<VirtualNodeID> vvid = {};
	node_table.add_storage_node(num_new_vnodes, sid, vvid);

	m.type = MSG_MANAGE_REPLY;
	m.node_id = sid;

	// copy to new node
	if (m.data) delete[] m.data;
	int num_vnodes = node_table.storage_nodes.size();
	m.data = new char[16 + 32 * num_vnodes];
	m.length = 1+sprintf(m.data, "%d", num_vnodes);
	for (auto& p : node_table.storage_nodes) {
		m.length += 1+sprintf(m.data + m.length, "%d %d", p.first, p.second);
	}
	
	// TODO
	unordered_map<StorageNodeID, vector<pair<VirtualNodeID, VirtualNodeID>>> donate_info;
	if (node_table.nodes.size() > CONFIG_N) {
		donate_info = donate_information(vvid);
	}

	m.owner = __func__;
	m.coordinator_id = donate_info.size();
	m.send(fd, m.data);
	m.recv(fd);

	// broadcast to old nodes
	fprintf(stderr, "\t%s: Broadcast new node %d to old storage nodes\n", __func__, sid);

	for (auto& x: node_table.nodes) {
		StorageNodeID nodeid = x.first;

		if (m.data) delete[] m.data;
		m.type = MSG_MANAGE_REPLY;
		m.data = new char[(1 + num_new_vnodes) * 16];
		m.length = 1+sprintf(m.data, "%d", num_new_vnodes);
		for (int i=0; i<num_new_vnodes; i++) {
			m.length += 1+sprintf(m.data + m.length, "%d", vvid[i]);
		}

		if (nodeid == sid) continue;
		int fd2 = openfd(storage_node_addr(nodeid).data());
		if (fd2<0){
			perror("ERROR: wrong fd\n");
			exit(1);
		}
		if (donate_info.count(nodeid))
			m.type |= DONATE_MASK;
		m.send(fd2, m.data);
		// if (node_table.nodes.size() > CONFIG_N && donate_info.count(nodeid)){
		// 	m.set_intervals(donate_info[nodeid]);
		// 	m.send(fd2, m.data);
		// }
		close(fd2);
	}

	m.recv(fd);
	fprintf(stderr, "<<< %s: Exiting\n", __func__);

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
		m.owner = ">>> Manager:Exec <<<";
		m.recv(connfd);
		if (m.type & CLIENT_MASK) {
			manage_client_request(m, connfd);
		} else if (m.type & FORWARD_MASK) {
			if (m.type & LEAVE_MASK)
				manage_node_request_leave(m, connfd);
			else
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
