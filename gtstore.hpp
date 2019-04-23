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

struct Message {
	int type;
	char client_id[MAX_ID_LENGTH];
	char node_id[MAX_ID_LENGTH];
	size_t length;
	char *data;

	ssize_t rio_writen(int fd, char* buf, size_t n) {
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
	Message(int t, char*cid, char*nid, int l):type(t), length(l){
		strcpy(client_id, cid);
		strcpy(node_id, nid);
		data = new char[length+1];
		data[length] = 0;
	}
	int send_to(char* addr){
		return 0;
	}
	Message(int fd){
		int offset = 0;
		char buf[256];
		int n;
		if ((n = read_line(fd, buf, sizeof(buf), &offset)) < 0){
			fprintf(stderr, "Failed in reading message\n");
			exit(-1);
		}
		sscanf(buf, "%d %s %s %d\n", &type, client_id, node_id, &length);
		data = new char[length+1];
		data[length] = 0;
		strncpy(data, buf+offset, n-offset);
		rio_readn(fd, data+n-offset, length-(n-offset));
	}
	~Message(){
		delete[] data;
	}
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
const string node_addr = "node.socket";
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
		bool operator=(Data& d) {
			version = d.version;
			value = d.value;
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