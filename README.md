# Coordinating Redundant OPC UA Servers

This code accompanies the IEEE ETFA'17 paper ["Coordinating Redundant OPC UA Servers"][1]. The paper is available under the docs folder and explains the architecture, data model, and implementation.

This code is for PoC purposes and should not be used in production.

### Compiling
Based on a debootstrapped debian stretch VM:

Dependencies:
```sh
apt-get install -y dh-autoreconf wget make git openjdk-8-jdk libcppunit-dev libcppunit-doc libtool patch libzookeeper-mt-dev libjansson-dev
```
Compile:
```sh
ACLOCAL="aclocal -I /usr/share/aclocal" autoreconf -if
./configure && make
```
### Running a zkUA Client, Server, Failover Controller
The ZooKeeper server/quorum should already be running.

Configure the zkUA configuration file (clientConf.txt and or serverConf.txt) and execute as so:
```sh
./cli_mt_UA_client
./cli_mt_UA_server
./cli_mt_UA_failoverController
```

The server should be started before the failoverController.

### Dockerfile
Build the docker image using:
```sh
docker build -t zkua .
```
Alternatively pull the image from:
```sh
docker pull amismail/zkuaRedundancy
```
Run the docker container using the command:
```sh
docker run -it -p 16664:16664 --name zkua_1 zkua /bin/bash
```
Port 16664 is the default port configured in serverConf.txt for the zkUA Server. 
This is the port you connect to using a zkUA Client or UA Client (e.g., using UA Expert).

NOTE: You will need a ZooKeeper cluster running - update the config files with ZooKeeper's IPs and port numbers accordingly.

### Limitations
Currently, the code is limited to replicating nodes with a namespace Index > 0 and nodes with numeric identifiers.

Methods are not replicated.

[1]:https://www.auto.tuwien.ac.at/~aismail/AI_K_ETFA17_Cc.pdf
