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
#include <unordered_map>

using namespace std;

typedef vector<string> val_t;

#define CONFIG_N	3
#define CONFIG_R	2
#define CONFIG_W	2

#define MAX_ID_LENGTH	32

#define CLIENT_MASK 1<<0
#define NODE_MASK 1<<1
#define COOR_MASK 1<<2
#define MANAGER_MASK 1<<3
#define WRITE_MASK 1<<4
#define REPLY_MASK 1<<5

typedef enum {
	MSG_CLIENT_REQUEST = CLIENT_MASK,
	MSG_CLIENT_REPLY = CLIENT_MASK | REPLY_MASK,
	MSG_NODE_REQUEST = NODE_MASK,
	MSG_NODE_REPLY = NODE_MASK | REPLY_MASK,
	MSG_COORDINATOR_REQUEST = COOR_MASK,
	MSG_COORDINATOR_REPLY = COOR_MASK | REPLY_MASK,
} message_type_t;

int open_clientfd(const char *addr);
ssize_t rio_writen(int fd, const char* buf, size_t n);
ssize_t rio_readn(int fd, char* buf, size_t n);
int read_line(int fd, char* buf, size_t n, int *loc);

struct Message {
	int type;
	char client_id[MAX_ID_LENGTH];
	char node_id[MAX_ID_LENGTH];
	size_t length;
	char *data=NULL;
	Message(int fd);
	Message(int t, const char *cid, const char *nid, int l);
	int send_to(int fd, const char *content);
	~Message();
	void print();
} ;



class GTStoreClient {
private:
	int client_id;
	val_t value;
public:
	void init(int id);
	void finalize();
	val_t get(string key);
	bool put(string key, val_t value);
};


typedef string VirtualNodeID;
typedef string StorageNodeID;
typedef string ClientID;

class NodeTable {
private:

public:
	hash<string>	consistent_hash;
	// hash<string>(s)
	map<size_t, VirtualNodeID>	virtual_nodes;
	map<size_t, StorageNodeID> 	storage_nodes;
	unordered_map<StorageNodeID, int>	socket_map;

	NodeTable(){};
	~NodeTable(){};

	vector<pair<VirtualNodeID, StorageNodeID>> get_preference_list(string key, int size=1);

};




const string node_addr = "node.socket";
constexpr char manager_addr[]="manager.socket";
constexpr int listenQ = 20;

class GTStoreManager {
public:
	int managerfd;
	void init();
	void exec();
};

class GTStoreStorage {
public:
	StorageNodeID	id;
	NodeTable	node_table;
	int nodefd;

	class Data {
	public:
		uint64_t version;
		string	 value;
		Data& operator=(Data& d) {
			version = d.version;
			value = d.value;
			return *this;
		}
	};
	map<VirtualNodeID, unordered_map<string, Data>>	data;


	void init();

	// data functions
	bool read_local(string key, Data& data, VirtualNodeID);
	bool write_local(string key, Data data, VirtualNodeID);

	// node functions
	StorageNodeID find_coordinator(string key);
	

	// coordinator functions
	bool read_remote(string key, Data& data, StorageNodeID, VirtualNodeID);
	bool write_remote(string key, Data data, StorageNodeID, VirtualNodeID);


	void exec();

	bool process_client_request(Message& msg);
	bool process_node_request(Message& msg);
	bool process_coordinator_request(Message& msg);
};

#endif