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

bool GTStoreStorage::read_local(string key, Data& data, VirtualNodeID v_id) {
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
bool GTStoreStorage::write_local(string key, Data data, VirtualNodeID v_id) {{
	if (this->data.count(v_id) == 0 ||
		this->data[v_id].count(key) == 0 ||
		this->data[v_id][key].version < data.version)
	{
		this->data[v_id][key] = data;
	}
	return true;
}}


StorageNodeID GTStoreStorage::find_coordinator(string key) {
	return node_table.get_preference_list(key, 1)[0].second;
}



bool GTStoreStorage::read_remote(string key, Data& data, StorageNodeID, VirtualNodeID) {
	
}


int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	
}