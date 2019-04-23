# Project 4 - GTStore
## Due: Tuesday April 23 11:59pm
In this project you will implement a distributed key-value store (GTStore) system. Nodes in this system should communicate with network calls. Please implement your program under the given skeleton codebase, do not add additional `.hpp` and `.cpp` files.

## Global Shared information
There are global shared info among manager and all storage nodes:
+ List of alive storage nodes
+ The virtual nodes to storage nodes address mapping

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

The client request manager for a contact node during initialization.


## Initial Implementation
+ Every message establish a new socket
+ Later improvement by event-driven model

"13 clientid nodeid length\ngchsyhesgeyjrhnjyadbyuj"

# References
+ Stevens, W. Richard, and Stephen A. Rago. Advanced programming in the UNIX environment. Addison-Wesley, 2008.
