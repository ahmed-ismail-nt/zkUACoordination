/*******************************************************************************
 * Copyright (C) 2018 Ahmed Ismail <aismail [at] protonmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <open62541.h>
#include <zookeeper.h>
#include <jansson.h>
#include <zk_jsonEncode.h>
#include <zk_jsonDecode.h>
//#include <zk_global.h>

typedef struct zkUA_locateParent {
    UA_NodeId *parentNode;
    UA_NodeId *searchedForNode;
    UA_NodeId *foundParent;
    UA_NodeId *referenceTypeId;
    UA_Boolean foundParentFlag;
} zkUA_locateParent;

typedef struct SetIfDifferentStruct {
    char *nodePath;
    char *dataToSet;
} SetIfDifferentStruct;

typedef struct zkUA_checkNs0 {
    UA_NodeId *searchedForNode;
    bool result;
} zkUA_checkNs0;

extern char zkServerAddressSpacePath[1024];
extern UA_Boolean availabilityPriority;
void zkUA_initializeAvailabilityPriority(UA_Boolean aPriority);
/**
 * zkUA_extractNsIndexNodeId:
 * Extracts a node's namespace index and nodeId substrings from a zk path string.
 */
void zkUA_extractNsIndexNodeId(const void *nId, char *storeNs, char *storeId);

/**
 * zkUA_UA_Server_replicateZk:
 * Runs zoo_aget_children for a server path and sends the results to
 * zkUA_UA_Server_replicateZk_getNodes for processing
 */
void zkUA_UA_Server_replicateZk(zhandle_t *zh, char *zkServerPath,
        UA_Server *uaServer);

/**
 * zkUA_UA_Server_replicateZk_getNodes:
 * Loops through a list of all the children znodes for a zk path
 * and calls zkUA_jsonDecode for each one
 */
void zkUA_UA_Server_replicateZk_getNodes(int rc,
        const struct String_vector *strings, const void *data);

/**
 * zkUA_checkMzxidAge:
 * Checks the Mzxid of the znode just acquired from zk to the mzxid of the UA Node stored
 * in the local cache.
 * Returns 1 if the mzxid of the UA Node stored is fresher than the data acquired from zk
 * Returns 0 if the mzxid is as fresh as the data acquired from zk.
 * Returns -1 if the mzxid comparison indicates that the data acquired from zk is fresher.
 */
int zkUA_checkMzxidAge(char *nodeZkPath, long long *mzxid);

/**
 * zkUA_insertMzxidAge:
 * Inserts the mzxid of a node just acquired from zk or just pushed to zk into the
 * local hashtable.
 */
UA_StatusCode zkUA_insertMzxidAge(char *nodeZkPath, long long *nodeMzxidLL);

/**
 * zkUA_deleteMzxidAge:
 * Deletes the mzxid for a specific znode from the local hashtable.
 */
UA_StatusCode zkUA_deleteMzxidAge(char *nodeZkPath);

/*
 * zkUA_jsonDecodeZkNode:
 * Gets the data of a single znode and calls a function to decode it back into OPC UA.
 */
UA_StatusCode zkUA_jsonDecode_zkNode(char * nodeZkPath,
        UA_Server *serverDecode);

/**
 * zkUA_jsonDecode_checkChildOfNS0ServerNode:
 * Checks if the supplied NodeId is a child of the namespace 0 Server Node.
 */
UA_StatusCode zkUA_jsonDecode_checkChildOfNS0ServerNode(UA_NodeId childId,
        UA_Boolean isInverse, UA_NodeId referenceTypeId,
        void *searchedForNodeId);

