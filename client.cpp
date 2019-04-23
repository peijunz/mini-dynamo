#include "gtstore.hpp"

// Utility functions

int open_clientfd(const char *addr){
	int fd;
	struct sockaddr_un sun;
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, addr);
	int len = offsetof(struct sockaddr_un, sun_path) + strlen(addr);
	if (connect(fd, (struct sockaddr *)&sun, len) < 0) {
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


Message::Message(int t, const char *cid, const char *nid, int l){
	type = t;
	length = l;
	strcpy(client_id, cid);
	strcpy(node_id, nid);
}

int Message::send(int fd, const char *content){
	char header[256];
	sprintf(header, "%d %s %s %ld\n", type, client_id, node_id, length);
	rio_writen(fd, header, strlen(header));
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
	sscanf(buf, "%d %s %s %ld\n", &type, client_id, node_id, &length);
	data = new char[length+1];
	data[length] = 0;
	strncpy(data, buf+offset, n-offset);
	rio_readn(fd, data+n-offset, length-(n-offset));
	return 0;
}

Message::~Message(){
	if (data!=NULL)
		delete[] data;
}

void Message::print(){
	printf("%d %s %s %ld\n", type, client_id, node_id, length);
	if (data){
		printf("%.*s\n\n", (int)length, data);
	}
};

////////////////////////////////////////////////////
void GTStoreClient::init(int id) {
	
	cout << "Inside GTStoreClient::init() for client " << id << "\n";
	client_id = id;
}

val_t GTStoreClient::get(string key) {
	
	cout << "Inside GTStoreClient::get() for client: " << client_id << " key: " << key << "\n";
	val_t value;
	// Get the value!
	return value;
}

bool GTStoreClient::put(string key, val_t value) {

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
