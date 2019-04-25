#include "gtstore.hpp"

void GTStoreStorage::init(int num_vnodes) {
	// TODO: Contact Manager to get global data


	// add to manager, get node id

    int fd = openfd(manager_addr);
    if (fd < 0){
        printf("error in nodefd\n");
        exit(-1);
    }
    Message m(MSG_FORWARD_REQUEST, -1, -1, 0);
	m.data = new char[m.length];
	m.length = sprintf(m.data, "%d", num_vnodes);
	m.owner = "GTStoreStorage::init";
    m.send(fd, m.data);
	printf("Sent request to manager\n");
    m.recv(fd);
	printf("Sent reply to manager\n");
	if (m.type & ERROR_MASK){
		printf("No available node\n");
		exit(1);
	}

	if (m.type != MSG_MANAGE_REPLY) {
		perror ("manager returns with unmatched message type!\n");
		exit(-1);
	}
	id = m.node_id;
	node_table.nodes.insert(id);
	char *cur = m.data;
	sscanf(cur, "%d", &num_vnodes);
	cur += strnlen(cur, 24)+1;
	VirtualNodeID vid;
	StorageNodeID sid;
	for (int i=0; i<num_vnodes; i++) {
		sscanf(cur, "%d %d", &vid, &sid);
		cur += strnlen(cur, 24)+1;
		node_table.add_virtual_node(vid);
		node_table.storage_nodes.insert({vid, sid});
		printf ("{%d, %d}\t", vid, sid);
		if (sid == id) {
			// my virtual node
			//size_t hash_key = node_table.consistent_hash(vid);
			data.insert({vid, {}});
		}
		else
			node_table.nodes.insert(sid);
	}
	printf("\n");

	printf ("Successfully add self to manager!  Node ID = %d\n", id);

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


    m.length = 0;
	m.type = 0;
	// m.node_id = -1;
	m.send(fd);

	printf("Inside GTStoreStorage::init() nodes %ld/%d\n", node_table.nodes.size(), CONFIG_N);
	if (node_table.nodes.size() > CONFIG_N)
		collect_tokens();

    m.length = 0;
	// m.node_id = -2;
	fprintf(stderr, "\t----ACK sending----\n");
	m.send(fd);
	fprintf(stderr, "\t----ACK sent----\n");
	close(fd);
}



void GTStoreStorage::collect_tokens(){
	printf(">>> Collecting tokens...\n");
	int todo = CONFIG_N;
	int connfd;
	vector<pair<int, int>> intervals;
	vector<pair<string, Data>> kvlist;
	while (todo){
		connfd = accept(nodefd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m;
		m.owner = __func__;
		m.recv(connfd);
		printf("\t~~~~~~~~~~~~~~~~~~~~~~~~~~~  Got token, todo=%d\n", todo);
		/*
		assert(m.type == MSG_DONATE_REQUEST);
		intervals = m.get_intervals();
		for (int i=0; i<(int)intervals.size(); i++){
			m.recv(connfd);
			kvlist = m.get_kv_list();
			// TODO: Update kvlist to own storage
			auto it = data.upper_bound(intervals[i].first);
			if (it == data.end()) it = data.begin();
			for (auto &x: kvlist){
				it->second.insert(x);
			}
		}
		*/
		kvlist = m.get_kv_list();
		for (auto& kv : kvlist) {
			VirtualNodeID vid = node_table.find_virtual_node(kv.first);
			this->data[vid].insert(kv);
		}
		todo--;
		close(connfd);
	}
	printf("<<< Done Collecting tokens...\n");

}

bool GTStoreStorage::read_local(VirtualNodeID v_id, string key, Data& data) {
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
bool GTStoreStorage::write_local(VirtualNodeID v_id, string key, Data data) {
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



// bool GTStoreStorage::read_remote(string key, Data& data, StorageNodeID, VirtualNodeID) {
// 	return false;
// }

void GTStoreStorage::leave() {
	return;
}

void GTStoreStorage::exec() {

	int connfd;
	while (1){
		connfd = accept(nodefd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m;
		m.owner = string("storage_") + to_string(id) + "_" + + __func__;
		m.recv(connfd);
		if (m.type & CLIENT_MASK) {
			// Because client does not listen, we do not
			// close client until we got a reply at process_forward_reply
			process_client_request(m, connfd);
		}
		else if (m.type & MSG_MANAGE_REPLY){
			process_manage_reply(m, connfd);
		}
		else{
			// Communication between storage nodes, sockets are disposable
			close(connfd);
			switch (m.type & (FORWARD_MASK|COOR_MASK|REPLY_MASK))
			{
				case MSG_FORWARD_REPLY:
					process_forward_reply(m);
					break;
				case MSG_FORWARD_REQUEST:
					process_forward_request(m);
					break;
				case MSG_COORDINATE_REPLY:
					process_coordinate_reply(m);
					break;
				case MSG_COORDINATE_REQUEST:
					process_coordinate_request(m);
					break;
			}
		}
	}
}

bool GTStoreStorage::process_client_request(Message& m, int fd) {
	// Find coordinator and forward request
	assert(node_table.nodes.size() >= CONFIG_N);
	// int offset = strnlen(m.data, m.length)+1;
	// char *val = m.data + offset;
	StorageNodeID sid = find_coordinator(m.data);
	forward_tasks[m.client_id] = fd;

	if (sid == id){
		// Do not forward, reply and then close
		// printf("\tContact is coordinatior\n");
		process_forward_request(m);
	}
	else{
		// Forward message
		m.type = MSG_FORWARD_REQUEST | (m.type & WRITE_MASK);
		int fwdfd = openfd(storage_node_addr(sid).data());
		if (fwdfd < 0){
			perror("forward fd");
		}
		m.owner = __func__;
		m.send(fwdfd, m.data);
		close(fwdfd);
	}
	return false;
}
bool GTStoreStorage::process_forward_request(Message& m) {
	// do as a coordinator. Broadcast R/W requests to pref_list
	printf(">>> Entered process_forward_request\n");
	string key;
	Data data;
	m.get_key_data(key, data);
	auto pref_list = node_table.get_preference_list(key, CONFIG_N);

	// initialize task
	working_tasks.insert({m.client_id, {0, Data()}});
	assert(working_tasks.count(m.client_id)!=0);
	// itnew_node
	pref_list.erase(pref_list.begin());
	if (m.type & WRITE_MASK) {
		// write
		write_local(m.vid, key, data);
		printf("\tWrite data %s\n", data.value.data());
	} else {
		// read
		read_local(m.vid, key, data);
		printf("\tRead data %s\n", data.value.data());
	}
	working_tasks[m.client_id].first++;
	working_tasks[m.client_id].second = data;

	// ask other nodes to complete their work
	m.coordinator_id = id;
	for (auto& pref : pref_list) {
		printf("\tBroadcast with proxy node_id...\n");
		m.type = MSG_COORDINATE_REQUEST | (m.type & WRITE_MASK);
		m.vid = pref.first;
		string coordinator_addr = node_addr + "_" + to_string(pref.second);
		int nodefd = openfd(coordinator_addr.data());
		if (nodefd < 0){
			printf("nodefd");
			exit(1);
		}
		m.owner = __func__;
		m.send(nodefd, m.data);
		close(nodefd);
	}
	if (working_tasks[m.client_id].first >= 
		((m.type & WRITE_MASK) ? CONFIG_W : CONFIG_R))
	{
		finish_coordination(m, key);
		printf("\tNo need to broadcast\n");
	}

	// printf("SIZE of working tasks for node %d is %d\n", id, working_tasks.size());
	printf("<<< Exited process_forward_request with todo=%d\n", working_tasks[m.client_id].first);
	return false;
}


bool GTStoreStorage::process_forward_reply(Message& msg) {
	assert(forward_tasks.count(msg.client_id)!=0);
	printf(">>> Send back to client\n");
	int clientfd = forward_tasks[msg.client_id];
	msg.type = MSG_CLIENT_REPLY;
	msg.owner = __func__;
	msg.send(clientfd, msg.data);
	close(clientfd);
	forward_tasks.erase(msg.client_id);
	return false;
}

bool GTStoreStorage::process_coordinate_request(Message& m) {
	// Do as coordinator asked to do

	string key;
	Data data;
	m.get_key_data(key, data);
	int node_fd = openfd(storage_node_addr(m.coordinator_id).data());
	if (node_fd < 0){
		perror("node fd");
		exit(1);
	}
	if (m.type & WRITE_MASK) {
		// write
		write_local(m.vid, key, data);
		m.type = MSG_COORDINATE_REPLY | (m.type & WRITE_MASK);
		m.owner = __func__;
		m.send(node_fd, m.data);
		close(node_fd);
	} 
	else {
		// read
		read_local(m.vid, key, data);
		m.type = MSG_COORDINATE_REPLY;
		m.set_key_data(key, data);
		m.owner = __func__;
		m.send(node_fd, m.data);
		close(node_fd);
	}
	return true;
}


bool GTStoreStorage::finish_coordination(Message &m, string &key){
	// task completed. 
	printf(">>> Entering %s\n", __func__);
	if (m.node_id == id) {
		printf("\titnew_node is the coordinator\n");
		m.set_key_data(key, working_tasks[m.client_id].second);
		printf("\tSIZE of forward tasks for node %d is %ld\n", id, forward_tasks.size());
		process_forward_reply(m);
	} else {
		printf("\tsend back to transferrer\n");
		m.type = MSG_FORWARD_REPLY | (m.type & WRITE_MASK);
		m.set_key_data(key, working_tasks[m.client_id].second);
		string transferrer_addr = node_addr + "_" + to_string(m.node_id);
		int nodefd = openfd(transferrer_addr.data());
		if (nodefd < 0){
			printf("error in nodefd\n");
			exit(-1);
		}
		m.owner = __func__;
		m.send(nodefd, m.data);
		close(nodefd);
	}

	// delete task
	working_tasks.erase(m.client_id);
	return 0;
}

bool GTStoreStorage::process_coordinate_reply(Message& m) {
	printf(">>> GTStoreStorage::process_coordinate_reply: Entering\n");
	if (working_tasks.count(m.client_id) == 0) {
		// task is already completed. Ignore redundant result
		printf("<<< Exiting %s\n", __func__);
		return false;
	}


	string key;
	Data data;
	m.get_key_data(key, data);
	working_tasks[m.client_id].first ++;

	if ((m.type & WRITE_MASK) ||
		data.version >= working_tasks[m.client_id].second.version)
	{
		// new version, update result
		working_tasks[m.client_id].second = data;
	}

	int target = (m.type & WRITE_MASK) ? CONFIG_W : CONFIG_R;
	printf("\tDone task %d/%d\n", working_tasks[m.client_id].first, target);
	if (working_tasks[m.client_id].first >= target)
	{
		finish_coordination(m, key);
	}

	return false;
}

bool GTStoreStorage::process_donate_request(Message& m) {
	/////// TODO:
	return false;
}



/*
void GTStoreStorage::collect_tokens(){
	printf(">>> Collecting tokens...\n");
	int todo = CONFIG_N;
	int connfd;
	vector<pair<int, int>> intervals;
	vector<pair<string, Data>> kvlist;
	while (todo){
		connfd = accept(nodefd, NULL, NULL);
    	if (connfd == -1) {
        	perror("Accept fail");
		}
		Message m;
		m.owner = __func__;
		m.recv(connfd);
		printf("\tGot token, todo=%d\n", todo);
		assert(m.type == MSG_DONATE_REQUEST);
		intervals = m.get_intervals();
		printf("\tGet Donation size=%ld\n", intervals.size());
		for (int i=0; i<(int)intervals.size(); i++){
			m.recv(connfd);
			kvlist = m.get_kv_list();
			// TODO: Update kvlist to own storage
			auto it = data.upper_bound(intervals[i].first);
			if (it == data.end()) it = data.begin();
			for (auto &x: kvlist){
				it->second.insert(x);
			}
		}
		todo--;
		close(connfd);
		sleep(1);
	}
	printf("<<< Done Collecting tokens...\n");

}*/

bool GTStoreStorage::process_manage_reply(Message& m, int fd) {
	
	m.owner = __func__;
	// a new node with id==m.node_id joins
	printf(">>> GTStoreStorage::process_manage_reply: Entering");
	m.owner = to_string(id) + "_" + __func__;
	node_table.nodes.insert(m.node_id);
	int num_vnodes;
	char*cur=m.data;
	sscanf(cur, "%d", &num_vnodes);
	cur += strnlen(cur, 24)+1;
	vector<VirtualNodeID> vvid(num_vnodes);
	printf ("\t%d join, %d vnodes\n", m.node_id, num_vnodes);
	printf ("\t%.*s\n", (int)m.length, m.data);
	assert(m.length);
	for (int i=0; i<num_vnodes; i++) {
		sscanf(cur, "%d", &vvid[i]);
		cur += strnlen(cur, 24)+1;
	}
	node_table.add_storage_node(num_vnodes, m.node_id, vvid);
	printf ("\tadd a new storage node !  Node ID = %d, New Node ID = %d\n", id, m.node_id);

	if (node_table.nodes.size() > CONFIG_N && (m.type & DONATE_MASK)){
		m.recv(fd);
		close(fd);
		vector<pair<VirtualNodeID, VirtualNodeID>> intervals = m.get_intervals();

		// assert (intervals.size()!=0);

		if (intervals.size() == 0) {
			return false;
		}

		int bootfd = openfd(storage_node_addr(m.node_id).data());
		if (bootfd < 0){
			perror("ERROR: connect boot fail");
			exit(1);
		}
		m.type = MSG_DONATE_REQUEST;
		//m.send(bootfd, m.data);
		vector<pair<string, Data>> kvlist;

		for (int i=0; i<(int)intervals.size(); i++){
			// Extract kv list
			auto it = data.upper_bound(intervals[i].first);
			if (it == data.end()) it = data.begin();
			if (it != data.end()) {
				for (auto jt=it->second. begin(); jt != it->second.end(); ) {
					if (node_table.find_virtual_node(jt->first) == intervals[i].second) {
						kvlist.push_back(*jt);
						jt = it->second.erase(jt);
					}
				}
			}
			// fprintf(stderr, "00000000000000000000  Get kv list: size=%ld\n", kvlist.size());

		
		}	
		// Send
		m.type = MSG_DONATE_REQUEST;
		m.set_kv_list(kvlist);
		m.print("\t~~~~~~~~~~~~~~~ Donate  Data in interval  ~~~~~~~~~~~~~~\n");
		m.send(bootfd, m.data);
		// m.print("\t[[[  Donate  Data in interval  ]]] \n");
		kvlist.clear();
		close(bootfd);
		// m.print("\t<<<  Donated  Interval  ]]] total: " + to_string(intervals.size()) + "\n");
	}
	
	//close(fd);

	// no enough nodes
	// if (node_table.nodes.size() <= CONFIG_N) {
	// 	return true;
	// }
	
	return true;
}


GTStoreStorage storage;

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
		storage.leave();
    }
}

int main(int argc, char **argv) {
    if(SIG_ERR == signal(SIGINT, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
        exit(1);
    }

    if(SIG_ERR == signal(SIGTERM, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
        exit(1);
    }
	storage.init();
	storage.exec();
}