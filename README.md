# Project 4 - GTStore
Peijun Zhu, Zilong Lyu

## Design

In this project we implemented a distributed key-value store (GTStore) system. Nodes in this system communicate with Unix Domain Socket. 

Our network topology as a fully connected graph among storage nodes and manager.

Because we potentially need to migrate virtual nodes among storage nodes, the data is stored in different sectors corresponding to different virtual nodes(token). In this way, getting new tokens or sending tokens is very easy.



## Data Principles

#### Partitioning

- Consistent hashing is used to partition data on different virtual nodes.
- Each storage node stores data of multiple virtual nodes.
- As storage nodes can dynamically join or leave the system, so data can be moved from one node to another. This process is called *donation*. 

#### Replication

- Each *key-value* pair is stored in $N$ **Storage** nodes. These nodes make up of a *preference list*.
- The replication parameter $N$ is 3 as default.

#### Consistency

- A *read* task is completed if at least $R$ nodes send results back to the coordinator.
- A *write* task is completed if at least $W$ nodes send results back to the coordinator.
- As long as $R/W$ results are collected, later results will be ignored.
- If multiple versions of results are retrieved, the latest version wins.
- The default value of $R$ and $W$ is 2.



## Request Processing

- **Forwarder**: When a client starts, it contact manager and manager allocates a contact storage node to this client. This contact storage node is called "forwarder". All requests from this client are sent to this forwarder, and all results are send back to client through the same forwarder. The forwarder can find out the *preference list*, and forwards the request to a node (called "coordinator") in the preference list. 
- **Coordinator**: The coordinator must be one node in the preference list. After getting a request forwarded by the forwarder, the coordinator processes the request by reading/writing its local data, and forwarding this request to other nodes on the *preference list*. Other nodes read/write their local data, and send results back to coordinator. After collecting enough responses to meet the R/W consistency, the coordinator sends results back to the forwarder and ignores later received results..
- Every node can be forwarder and coordinator. For a specific request, the coordinator can also be the forwarder itself. 



## Communication

- All manager, storage nodes and clients communicate with each other by *Unix Domain Socket*. The manager has a fixed address known by all nodes and clients.
- A new socket is established for every procedure. A procedure could be: 
  - a client sends a request to its forwarder; 
  - a forwarder forwards a request to a coordinator; 
  - a coordinator forwards a request to neighboring nodes; 
  - a node notifies manager when joining/leaving;
  - manager notifies a node to update information; 
  - a storage node donates data to another storage node.
- Manager and storage nodes run a local finite state machine, which performs different functions according to the types of received messages and its current state.



## Membership

- There must be at least N storage nodes running. Otherwise the replication principle cannot be met, and no request will be accepted.
- When a new storage node joins, it first contact manager to get global information such as storage node ids and virtual nodes. The manager notifies all old nodes to update their local information. The manager determines Which the old nodes should donate some data to the new nodes. This determination is done by a ***Two Pointer*** traverse of the consistent hashing ring ($O(n)​$ time complexity). The manager sends a "donate" request to these nodes, and then these nodes send "donated" data to the new node.
- When a node is going to leave, it uses its local copy of global information to determine which storage nodes should take over its work. Then it sends(donates) the data on its own virtual nodes to those storage nodes.



## Load balancing

- Client gets recommended contact node from manager, which is given by round-robin
- More virtual nodes could guarantee keys are balanced among storage nodes





## Components

#### Centralized Manager

+ The manager maintains global information: alive virtual/storage nodes, mapping from virtual nodes to storage nodes. 

- The manager creates unique ids for each client and storage nodes.
- The manager determines who should donate its data if there is a new node joining.
- The manager notifies all storages nodes when a node is joining or leaving.

#### Storage Node

- Each storage node maintains a copy of global information identical to the manager.
- Each storage node maintains $V$ virtual nodes ()$V=3$ by default).
- Each storage node stores *key-value* mappings locally.

#### Client

- Client needs to contact manager to get its id and forwarder.
- Client need not to store or keep track of global information.
- Client cannot put forward a read/write request before completion of  current request.
- Client has the following APIs:
  - *init()*: contacts to manager; gets client id and the contact node (forwarder).
  - *put()*: sends a *write* request to forwarder, and then waits for reply of completion. 
  - *get()*: sends a *read* request to forwarder, and then waits for the reply of completion with result.
  - *finalize()*: pre-exit cleaning.



## Client Driver Application

#### Test

In ```test_app.cpp```, we do the following things:

- Start manager.
- Start $2N$ storage nodes ($N$ = Replication parameter).
- Start $K$ clients, each sending a *put()* request.
- For each storage node, let it leave the system and then start a new storage nodes. Finally we replace $2N$ old storage nodes one by one with $2N$ new storage nodes.
- For each client, send a *get()* request with the same *key* it puts.
- Let all storage nodes leave the system.
- End all clients and manager.

#### Functionality

In this test, we can verify following functionalities:

- 







## Design Tradoffs

#### Pros

- Client does not need to store or keep track of global information. Every client is allocated to a fixed storage node to communicate. This reduces traffics between clients and manager, which benefits a lot when there are a huge number of clients/storage nodes.
- All storage nodes have copies of global information. Based on local information, every storage node can find *preference list* and know where to put data by itself, which reduces a lot of communication between storage nodes and managers.

- Manager always keeps track of global information and notifies storage nodes when update is required. This helps keep state consistency among all storage nodes.

#### Cons

- 





# References
+ Stevens, W. Richard, and Stephen A. Rago. Advanced programming in the UNIX environment. Addison-Wesley, 2008.

