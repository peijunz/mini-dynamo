#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
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

struct Message {
	message_type_t type;
	char client_id[MAX_ID_LENGTH];
	char node_id[MAX_ID_LENGTH];
	size_t length;
	char data[];
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
	static const hash<string>	consistent_hash;

public:
	map<size_t, VirtualNodeID>	virtual_nodes;
	map<size_t, StorageNodeID> 	storage_nodes;
	unordered_map<StorageNodeID, int>	socket_map;

	NodeTable(){};
	~NodeTable(){};

	vector<pair<VirtualNodeID, StorageNodeID>> get_preference_list(string key, int size=1);

};




constexpr char* manager_addr="manager.socket";
constexpr int listenQ = 20;

class GTStoreManager {
public:
	int managerfd;
	void init();
};

class GTStoreStorage {
public:
	StorageNodeID	id;
	NodeTable	node_table;

	class Data {
	public:
		uint64_t version;
		string	 value;
		bool operator=(Data& d) {
			version = d.version;
			value = d.value;
		}
	};
	map<VirtualNodeID, unordered_map<string, Data>>	data;


	void init();

	// storage node functions
	bool read_local(string key, Data& data, VirtualNodeID);
	bool write_local(string key, Data data, VirtualNodeID);

	// node functions
	StorageNodeID find_coordinator(string key);
	

	// coordinator functions
	bool read_remote(string key, Data& data, StorageNodeID, VirtualNodeID);
	bool write_remote(string key, Data data, StorageNodeID, VirtualNodeID);

};

#endif
