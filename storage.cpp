#include "gtstore.hpp"



vector<pair<VirtualNodeID, StorageNodeID>> NodeTable::get_preference_list(string key, int size=1) {
	size_t hash_key = consistent_hash(key);
	auto it = virtual_nodes.upper_bound(hash_key);
	vector<pair<VirtualNodeID, StorageNodeID>>	pref_list;
	for (int i=0; i<size; i++) {
		if (it == virtual_nodes.end())
			it = virtual_nodes.begin();
		pref_list.push_back({
			it->second, 				// virtual node id
			storage_nodes[it->first]	// storage node id
		});
	}
	return pref_list;
}






void GTStoreStorage::init() {
	
	cout << "Inside GTStoreStorage::init()\n";
}

int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	
}