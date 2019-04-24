#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cassert>
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
#include <unordered_set>

using namespace std;

typedef string val_t;

#define CONFIG_V	3
#define CONFIG_N	5
#define CONFIG_R	3
#define CONFIG_W	3

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

#define CLIENT_MASK 1<<0
#define FORWARD_MASK 1<<1
#define COOR_MASK 1<<2
#define MANAGE_MASK 1<<3
#define WRITE_MASK 1<<4
#define REPLY_MASK 1<<5
#define DONATE_MASK 1<<6
#define ERROR_MASK 1<<8


typedef int VirtualNodeID;
typedef int StorageNodeID;
typedef int ClientID;


typedef enum {
	MSG_CLIENT_REQUEST = CLIENT_MASK,
	MSG_CLIENT_REPLY = CLIENT_MASK | REPLY_MASK,
	MSG_MANAGE_REPLY = MANAGE_MASK | REPLY_MASK,
	MSG_FORWARD_REQUEST = FORWARD_MASK,
	MSG_FORWARD_REPLY = FORWARD_MASK | REPLY_MASK,
	MSG_COORDINATE_REQUEST = COOR_MASK,
	MSG_COORDINATE_REPLY = COOR_MASK | REPLY_MASK,
	MSG_DONATE_REQUEST = DONATE_MASK
} message_type_t;

string typestr(int type);
int openfd(const char *addr);
ssize_t rio_writen(int fd, const char* buf, size_t n);
ssize_t rio_readn(int fd, char* buf, size_t n);
int read_line(int fd, char* buf, size_t n, int *loc);

struct Message {
	string owner="";
	int type;
	ClientID client_id;
	StorageNodeID node_id;
	StorageNodeID coordinator_id=-1;
	VirtualNodeID vid=-1;
	size_t length;
	char *data=NULL;
	int set(int t, int cid, int nid, int l);
	int send(int fd, const char *content=NULL);
	int recv(int fd);
	Message(){}
	Message(int t, int cid, int nid, int l){set(t, cid, nid, l);}
	~Message();
	void print(string);

	int set_key_data(string key, Data data);
	int get_key_data(string& key, Data& data);

	int set_kv_list(vector<pair<string, Data>>&);
	vector<pair<string, Data>> get_kv_list();

	int set_intervals(vector<pair<VirtualNodeID, VirtualNodeID>>& intervals);
	vector<pair<VirtualNodeID, VirtualNodeID>> get_intervals();
};


class GTStoreClient {
private:
	int get_contact_node(int id);
	int connect_contact_node();
	int node_id; // Major contact node for this client
public:
	int client_id;
	val_t value;
	void init(int id);
	void finalize();
	val_t get(string key);
	bool put(string key, val_t value);

};


class NodeTable {
public:
	hash<string>	consistent_hash;			// hash<string>(s)

	// node map
	int num_virtual_nodes = 0;
	int num_storage_nodes = 0;
	map<size_t, VirtualNodeID>	virtual_nodes;
	map<VirtualNodeID, StorageNodeID> 	storage_nodes;

	unordered_set<StorageNodeID> 	nodes;
	

	NodeTable(){};
	~NodeTable(){};

	void add_virtual_node(VirtualNodeID vid = -1);
	void add_storage_node(int num_vnodes, StorageNodeID& sid, vector<VirtualNodeID>& vvid);

	VirtualNodeID find_virtual_node(string key);
	VirtualNodeID find_neighbor_virtual_node(VirtualNodeID);

	vector<pair<VirtualNodeID, StorageNodeID>> get_preference_list(string key, int size=1);

};


#define storage_node_addr(id) ("node.socket_" + to_string(id))
#define virtual_node_addr(id) ("virtual_node_" + to_string(id))

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

class compare {
public:
	bool operator() (const VirtualNodeID& v1,  const VirtualNodeID& v2) const {
		return hash<string>{}(virtual_node_addr(v1)) < hash<string>{}(virtual_node_addr(v2));
	}
};

class GTStoreStorage {
public:
	StorageNodeID	id=-1;
	NodeTable	node_table;
	int nodefd;


	map<VirtualNodeID, unordered_map<string, Data>, compare>	data;

	// tasks
	unordered_map<ClientID, int>				forward_tasks;	// task --> socket
	unordered_map<ClientID, pair<int, Data>>	working_tasks;	// task --> received results

	void init(int num_vnodes=CONFIG_V);

	void steal_token(VirtualNodeID);

	// data functions
	bool read_local(VirtualNodeID vid, string key, Data& data);
	bool write_local(VirtualNodeID vid, string key, Data data);

	// node functions
	StorageNodeID find_coordinator(string key);
	

	// coordinator functions
	// bool read_remote(string key, Data& data, StorageNodeID, VirtualNodeID);
	// bool write_remote(string key, Data data, StorageNodeID, VirtualNodeID);


	void exec();
	// Exclusive connection to client/manager
	bool process_client_request(Message& msg, int fd);
	bool process_manage_reply(Message& msg, int fd);
	//// TODO: leave



	// Communication between nodes
	bool process_forward_request(Message& msg);
	bool process_forward_reply(Message& msg);
	bool process_coordinate_request(Message& msg);
	bool process_coordinate_reply(Message& msg);
	bool finish_coordination(Message &m, string &key);

	bool process_donate_request(Message& m) ;
};

#endif