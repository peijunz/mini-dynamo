#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

#include <map>
#include <unordered_map>

using namespace std;

typedef vector<string> val_t;





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





class GTStoreManager {
public:
	void init();
};

class GTStoreStorage {
public:
	StorageNodeID	id;
	map<VirtualNodeID, unordered_map<string, string>>	data;
	NodeTable	node_table;

	void init();
};

#endif