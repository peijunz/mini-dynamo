#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <map>
#include <set>
#include <unordered_map>

using namespace std;

typedef vector<string> val_t;

#define CONFIG_N	3
#define CONFIG_R	2
#define CONFIG_W	2

#define MAX_ID_LENGTH	32
#define MAX_KEY_LENGTH	20

class Data {
public:
	uint64_t version = 0;
	string	 value = "";
	Data& operator=(Data& d) {
		version = d.version;
		value = d.value;
		return *this;
	}
	int get_length() {
		return 64 + value.size() + 1;
	}
};

#define ERROR_MASK 1<<8
#define CLIENT_MASK 1<<0
#define NODE_MASK 1<<1
#define COOR_MASK 1<<2
#define MANAGER_MASK 1<<3
#define WRITE_MASK 1<<4
#define REPLY_MASK 1<<5



typedef int VirtualNodeID;
typedef int StorageNodeID;
typedef int ClientID;


typedef enum {
	MSG_CLIENT_REQUEST = CLIENT_MASK,
	MSG_CLIENT_REPLY = CLIENT_MASK | REPLY_MASK,
	MSG_NODE_REQUEST = NODE_MASK,
	MSG_MANAGER_REPLY = MANAGER_MASK | REPLY_MASK,
	MSG_NODE_REPLY = NODE_MASK | REPLY_MASK,
	MSG_COORDINATOR_REQUEST = COOR_MASK,
	MSG_COORDINATOR_REPLY = COOR_MASK | REPLY_MASK,
} message_type_t;

int openfd(const char *addr);
ssize_t rio_writen(int fd, const char* buf, size_t n);
ssize_t rio_readn(int fd, char* buf, size_t n);
int read_line(int fd, char* buf, size_t n, int *loc);

struct Message {
	int type;
	ClientID client_id;
	StorageNodeID node_id;
	size_t length;
	char *data=NULL;
	int set(int t, int cid, int nid, int l);
	int send(int fd, const char *content=NULL);
	int recv(int fd);
	Message(){}
	Message(int t, int cid, int nid, int l){set(t, cid, nid, l);}
	~Message();
	void print();

	int set_key_data(string key, Data data);
	int get_key_data(string& key, Data& data);
} ;



class GTStoreClient {
public:
	int client_id;
	int node_id; // Major contact node for this client
	val_t value;
	void init(int id);
	void finalize();
	val_t get(string key);
	bool put(string key, string value);
};


class NodeTable {
private:

public:
	hash<string>	consistent_hash;			// hash<string>(s)

	// node map
	int num_virtual_nodes = 0;
	int num_storage_nodes = 0;
	map<size_t, VirtualNodeID>	virtual_nodes;
	map<VirtualNodeID, StorageNodeID> 	storage_nodes;
	

	NodeTable(){};
	~NodeTable(){};

	void add_virtual_node(VirtualNodeID vid = -1);
	void add_storage_node(int num_vnodes, StorageNodeID& sid, vector<VirtualNodeID>& vvid);

	VirtualNodeID find_virtual_node(string key);

	vector<pair<VirtualNodeID, StorageNodeID>> get_preference_list(string key, int size=1);

};


#define storage_node_addr(id) (node_addr + "_" + to_string(id))

const string node_addr = "node.socket";
constexpr char manager_addr[]="manager.socket";
constexpr int listenQ = 20;

class GTStoreManager {
public:
	int managerfd;
	NodeTable node_table;
	void init();
	void exec();
	int cur_contact=-1;
	set<StorageNodeID>	storage_nodes;
	int manage_client_request(Message& m, int fd);
	int manage_node_request(Message& m, int fd);
};

class GTStoreStorage {
public:
	StorageNodeID	id=-1;
	NodeTable	node_table;
	int nodefd;

	map<VirtualNodeID, unordered_map<string, Data>>	data;

	// tasks
	unordered_map<ClientID, int>	forward_tasks;	// task --> socket
	unordered_map<ClientID, Data>	working_tasks;	// task --> received results

	void init(int num_vnodes=CONFIG_N);

	// data functions
	bool read_local(string key, Data& data);
	bool write_local(string key, Data data);

	// node functions
	StorageNodeID find_coordinator(string key);
	

	// coordinator functions
	bool read_remote(string key, Data& data, StorageNodeID, VirtualNodeID);
	bool write_remote(string key, Data data, StorageNodeID, VirtualNodeID);


	void exec();

	bool process_client_request(Message& msg, int fd);
	bool process_node_request(Message& msg, int fd);
	bool process_node_reply(Message& msg, int fd);
	bool process_coordinator_request(Message& msg, int fd);
	bool process_coordinator_reply(Message& msg, int fd);
	bool process_manager_reply(Message& msg, int fd);
};

#endif