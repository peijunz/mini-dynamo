# Project 4 - GTStore
## Due: Tuesday April 23 11:59pm
In this project you will implement a distributed key-value store (GTStore) system. Nodes in this system should communicate with network calls. Please implement your program under the given skeleton codebase, do not add additional `.hpp` and `.cpp` files.

## Design

#### Global Topology

Consider our network topology as a fully connected graph among storage nodes and manager.
For every storage node and manager, it also stores a list of open socket to other storage nodes. 

Because we potentially need to migrate virtual nodes among storage nodes, the data is stored in different sectors corresponding to different virtual nodes(token). In this way, getting new tokens or sending tokens is very easy.

#### Data Principles

- Partitioning
  - We use consistent hashing to partition data on different virtual nodes.
  - Each storage node stores data of some virtual nodes.

- Replication
  - Each *key-value* pair is stored in N **Storage** nodes.
  - The replication parameter N is 3 as default.

- Consistency
  - A *read* task is completed if at least R nodes send results back to the coordinator.
  - A *write* task is completed if at least W nodes send results back to the coordinator.
  - The default value of R and W is 2.

#### Forward-Coordinate

- Forward: The forwarder gets a task requested from a client. The forwarder find a coordinator and then forwards the task to the coordinator . After collecting enough results, the coordinator sends results back to forwarder. The forwarder sends results back to the client.
- Coordinate: The coordinator gets a task from a forwarder. Then the coordinator reads/writes its local data, and sends this task to other nodes on the preference list. Other nodes send results back to coordinator. After collecting enough responses to meet consistency, the coordinator sends results back to the forwarder.
- The forwarder and coordinator can be the same node. In this case, we don't forward task to itself.

#### Membership

- There must be at least N storage nodes running. Otherwise no request is accepted.
- When a new node joins, it first contact manager to get its id and virtual nodes. The manager notifies all old nodes to update their local information. The manager determines who among the old nodes should donate some data to the new nodes. The manager sends "donate" request to these nodes, and these nodes send "donated" data to the new node.
- When a node plans to leave, it uses its local copy of global information to determine which storage nodes should take over its virtual nodes. Then it sends the data on these virtual nodes to those storage nodes.

#### Load balancing

- Client gets recommended contact node from manager, which is given by round-robin
- Virtual nodes itself could guarantee keys are balanced among storage nodes





## Global Topology

Consider our network topology as a fully connected graph among storage nodes and manager.
For every storage node and manager, it also stores a list of open socket to other storage nodes. 

Because we potentially need to migrate virtual nodes among storage nodes, the data is stored in different sectors corresponding to different virtual nodes(token). In this way, getting new tokens or sending tokens is very easy.

## Membership
+ When a storages nodes starts, it first contacts manager to get existing global information, and then send its existence to all other nodes. Then it may gather tokens from its neighbors.
+ When a node leaves, it also contacts manager and other neighbors to give its data. Then it must update its leaving info mation to all nodes. 

## Load balancing
+ Client get recommended node from manager, which is given by round-robin
+ Virtual nodes itself could guarantee keys are balanced among storage nodes

## Request of Clients
> There are two strategies that a client can use to select a node: (1) route its request through a generic load balancer that will select a node based on load information, or (2) use a partition-aware client library that routes requests directly to the appropriate coordinator nodes. 

The client request manager for a contact node during initialization. We call this node forwarder. The client sends all request to, and receives all results from the same forwarder.





## Components

#### Centralized Manager

+ The manager maintains global information: alive virtual/storage nodes, mapping from virtual nodes to storage nodes. 

- The manager creates unique ids for each client and storage nodes.
- The manager determines who should donate its data if there is a new node joining.

#### Storage Node

- Each storage node maintains a copy of global information  identical to the manager.
- Each storage node maintains multiple virtual nodes.
- Each storage node stores *key-value* mapping locally.

#### Client

- *init()*: contacts to manager, gets client id and the contact node. We call the contact node "forwarder"
- *put()*: sends a *write* request to contact node, and then waits for the reply of completion. 
- *get()*: sends a *read* request to contact node, and then waits for the reply of *value*.
- *finalize()*: pre-exit.





- 

## Client Driver Application

- 
















## Initial Implementation
+ Every message establish a new socket
+ Later improvement by event-driven model

"13 clientid nodeid length\ngchsyhesgeyjrhnjyadbyuj"

# References
+ Stevens, W. Richard, and Stephen A. Rago. Advanced programming in the UNIX environment. Addison-Wesley, 2008.







0 1 2 3 4 5 5 5 4 3 2 1 0