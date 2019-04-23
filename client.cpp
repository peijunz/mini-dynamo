#include "gtstore.hpp"

// Utility functions

int openfd(const char *addr){
	int fd;
	struct sockaddr_un sun;
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror ("Create socket error");
		return -1;
	}
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, addr);
	int len = offsetof(struct sockaddr_un, sun_path) + strlen(addr);
	if (connect(fd, (struct sockaddr *)&sun, len) < 0) {
		perror ("Socket connect error");
		return -1;
	}
	return fd;
}

ssize_t rio_writen(int fd, const char* buf, size_t n) {
	size_t rem = n;
	ssize_t nwrite = 0;
	while(rem > 0) {
		if((nwrite = write(fd, buf, rem)) <= 0) { //EINTER
			if(errno == EINTR)
				nwrite = 0;
			else {
				perror("write");
				return -1;
			}
		}
		rem -= (size_t)nwrite;
		buf += nwrite;
	}
	return (ssize_t)n;
}

ssize_t rio_readn(int fd, char* buf, size_t n) {
	size_t rem = n;
	ssize_t nread;
	while(rem > 0) {
		if((nread = read(fd, buf, rem)) < 0) {
			if(errno == EINTR)
				nread = 0;
			else
				return -1;
		} else if(nread == 0)
			break;
		rem -= (size_t)nread;
		buf += nread;
	}
	return (ssize_t)(n - rem);
}

int read_line(int fd, char* buf, size_t n, int *loc) {
	size_t rem = n;
	ssize_t nread;
	while(rem > 0) {
		if((nread = read(fd, buf, rem)) < 0) {
			if(errno == EINTR)
				nread = 0;
			else
				return -1;
		} else if(nread == 0)
			break;
		rem -= (size_t)nread;
		buf += nread;
		for (char *p=buf-nread; p < buf; p++){
			if (*p=='\n'){
				*loc = n-rem-(buf-p)+1;
				return (ssize_t)(n - rem);
			}
		}
	}
	return (ssize_t)(n - rem);
}

//////////////////  Message Send/Recv  //////////////////////


int Message::set(int t, int cid, int nid, int l){
	type = t;
	length = l;
	client_id = cid;
	node_id = nid;
	return 0;
}

int Message::send(int fd, const char *content){
	char header[256];
	sprintf(header, "%d %d %d %ld\n", type, client_id, node_id, length);
	rio_writen(fd, header, strlen(header));
	if (content)
		rio_writen(fd, content, length);
	return 0;
}

int Message::recv(int fd){
	if (data!=NULL)
		delete[] data;
	int offset = 0;
	char buf[256];
	int n;
	if ((n = read_line(fd, buf, sizeof(buf), &offset)) < 0){
		fprintf(stderr, "Failed in reading message\n");
		exit(-1);
	}
	sscanf(buf, "%d %d %d %ld\n", &type, &client_id, &node_id, &length);
	// if (length>0){
	data = new char[length+1];
	data[length] = 0;
	strncpy(data, buf+offset, n-offset);
	rio_readn(fd, data+n-offset, length-(n-offset));
	// }
	return 0;
}

Message::~Message(){
	if (data!=NULL)
		delete[] data;
}

void Message::print(){
	printf("%d %d %d %ld\n", type, client_id, node_id, length);
	if (length && data){
		printf("%.*s\n\n", (int)length, data);
	}
};

int Message::set_key_data(string key, Data data) {
	if (key.size() > MAX_KEY_LENGTH)
		return -1;
	length = MAX_KEY_LENGTH+1 + 64 + data.get_length();
	if (this->data) delete[] this->data;
	this->data = new char[length];
	snprintf(this->data, MAX_KEY_LENGTH+1, "%s", key.data());
	sprintf(this->data + MAX_KEY_LENGTH+1, "%lld %s\n", data.version, data.value.data());
	return 0;
}
int Message::get_key_data(string& key, Data& data) {
	sscanf(this->data, "%.*s", MAX_KEY_LENGTH+1, key.data());
	sscanf(this->data + MAX_KEY_LENGTH+1, "%lld %s\n", &data.version, &data.value);
	return 0;
}








///////////////    Node  Table




void NodeTable::add_virtual_node(VirtualNodeID vid) {
	if (vid == -1)
		vid = num_virtual_nodes ++;
	size_t hash_key = consistent_hash("virtual_node_" + to_string(vid));
	virtual_nodes.insert({hash_key, vid});
	printf ("\tvid=%d", vid);
}

void NodeTable::add_storage_node(int num_vnodes, StorageNodeID& sid, vector<VirtualNodeID>& vvid) {
	if (sid == -1) {
		sid = num_storage_nodes ++;
		vvid.clear();
		for (int i=0; i<num_vnodes; i++)
			vvid.push_back(num_virtual_nodes ++);
	}
	printf ("sid=%d\n", sid);
	for (VirtualNodeID vid : vvid) {
		add_virtual_node(vid);
		storage_nodes.insert({vid, sid});
	}
	printf ("\n");
}


VirtualNodeID NodeTable::find_virtual_node(string key) {
	size_t hash_key = consistent_hash(key);
	auto it = virtual_nodes.upper_bound(hash_key);
	if (it == virtual_nodes.end())
		it = virtual_nodes.begin();
	return it->second;
}

vector<pair<VirtualNodeID, StorageNodeID>> NodeTable::get_preference_list(string key, int size) {
	size_t hash_key = consistent_hash(key);
	auto it = virtual_nodes.upper_bound(hash_key);
	unordered_map<StorageNodeID, VirtualNodeID>	pref_map;
	vector<pair<VirtualNodeID, StorageNodeID>>	pref_list;
	int cycle=0;
	while (pref_map.size()<size) {
		if (it == virtual_nodes.end()){
			if (cycle ==1){
				// Infinite loop
				break;
			}
			it = virtual_nodes.begin();
			cycle += 1;
		}
		if (pref_map.count(storage_nodes[it->second]) == 0)
			pref_map[storage_nodes[it->second]] = it->second;
		it++;
	}
	for (auto const& x :pref_map) {
		pref_list.push_back({
			x.second, 				// virtual node id
			x.first	// storage node id
		});
	}
	return pref_list;
}















////////////////////////////////////////////////////
void GTStoreClient::init(int id) {
	
	cout << "Inside GTStoreClient::init() for client " << id << "\n";
	client_id = id;
	char buf[32];
    int fd = openfd(manager_addr);
    if (fd < 0){
        printf("error in clientfd\n");
        exit(-1);
    }
    Message msg(MSG_CLIENT_REQUEST, client_id, -1, 0);
    msg.send(fd);
    msg.recv(fd);
	close(fd);
	if (msg.type & ERROR_MASK){
		printf("No available node\n");
		//exit(1);
	}
	node_id = msg.node_id;
	printf("Got contact %d\n", node_id);
}

val_t GTStoreClient::get(string key) {
	// Attempt to find its contact
	// If failed, then ask manager for a new contact
	
	// send message to contact node
	Data data;
	Message msg(MSG_CLIENT_REQUEST, client_id, node_id, MAX_KEY_LENGTH+1 + data.get_length());
	msg.data = new char[msg.length];
	msg.set_key_data(key, data);
	string nodeaddr = node_addr + "_" + to_string(node_id);
	int fd = openfd(nodeaddr.data());
    if (fd < 0){
        printf("error in clientfd\n");
        exit(-1);
    }
	msg.send(fd, msg.data);
	delete[] msg.data;
	
	close(fd);


	// receive data
	


	cout << "Inside GTStoreClient::get() for client: " << client_id << " key: " << key << "\n";
	val_t value;
	// Get the value!
	return value;
}

bool GTStoreClient::put(string key, string value) {
	// Attempt to find its contact
	// If failed, then ask manager for a new contact

	string print_value = "";
	for (uint i = 0; i < value.size(); i++) {
		print_value += value[i] + " ";
	}
	cout << "Inside GTStoreClient::put() for client: " << client_id << " key: " << key << " value: " << print_value << "\n";
	// Put the value!
	return true;
}

void GTStoreClient::finalize() {
	
	cout << "Inside GTStoreClient::finalize() for client " << client_id << "\n";
}
