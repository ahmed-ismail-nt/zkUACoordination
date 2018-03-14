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
#include <open62541.h>
#include <zookeeper.h>
#include <jansson.h>

/***** INTERCEPTED FUNCTIONS *****/

/***** Node Deletion Functions *****/
/**
 * zkUA_Service_DeleteNodes_single:
 * Interception of Service_DeleteNodes_single which is called whenever a node is deleted.
 * If replication to zk is desired it also deletes the node from zk's addressSpace
 * otherwise it is only deleted from the UA server cache.
 */
UA_StatusCode
zkUA_Service_DeleteNodes_single(UA_Server *server, UA_Session *session,
        const UA_NodeId *nodeId, UA_Boolean deleteReferences);
/**
 * zkUA_UA_Server_deleteNode_dontReplicate:
 * Use this in your program if you want to call Service_DeleteNodes_single and the replication
 * of node deletes to ZooKeeper are enabled by default.
 */
UA_StatusCode zkUA_UA_Server_deleteNode_dontReplicate(UA_Server *server,
        const UA_NodeId nodeId, UA_Boolean deleteReferences);

/***** Attribute Reading Functions *****/
/**
 * zkUA_Service_Read_single:
 * Used to intercept all calls to Service_Read_single.
 * Service_Read_single is the open62541 library function called to read
 * attributes regardless of the source of the request: i.e. from within the
 * UA Server application or by a UA client over the network.
 * Interception is included to ensure that zk is the only source of truth in the redundancyGroup
 * unless the availabilityPriority global variable is set at startup via the config file.
 * If it is set and zk is unreachable the local cache is used for reads.
 */
void zkUA_Service_Read_single(UA_Server *server, UA_Session *session,
        const UA_TimestampsToReturn timestamps, const UA_ReadValueId *id,
        UA_DataValue *v);

/***** Attribute Writing Functions *****/
/**
 * zkUA_UA_Server_write:
 * Used to intercept all calls to UA_Server_write.
 * UA_Server_write is the open62541 library function called by a UA
 * Server application to edit (update/write) a node's attribute.
 */
UA_StatusCode zkUA_UA_Server_write(UA_Server *server,
        const UA_WriteValue *value);

/**
 * zkUA_Service_Write:
 * Used to intercept all calls to Service_Write.
 * Service_Write is the open62541 library function called if a
 * UA Server receives a request edit a node's attribute from a
 * UA Client over the network.
 */
void zkUA_Service_Write(UA_Server *server, UA_Session *session,
        const UA_WriteRequest *request, UA_WriteResponse *response);

/***** Node Addition Functions *****/

/**
 * zkUA_Service_AddNodes_single:
 * Used to intercept all calls to Service_AddNodes_single
 * Service_AddNodes_single is the open62541 library function called if a
 * UA Server application adds a node by its own volition or if
 * it receives a request to add a node from a UA Client over the network.
 */
void zkUA_Service_AddNodes_single(UA_Server *server, UA_Session *session,
        const UA_AddNodesItem *item, UA_AddNodesResult *result,
        UA_InstantiationCallback *instantiationCallback);
/**
 * zkUA_addNodeInternal:
 * Used to intercept all calls to  addNodeInternal.
 * addNodeInternal is the open62541 library function called at
 * UA Server startup to initialize namespace Index 0 nodes.
 */
UA_AddNodesResult zkUA_addNodeInternal(UA_Server *server, UA_Node *node,
        const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId);

/********** SUPPORTING FUNCTIONS **********/
/***** Functions supporting replication during code interception *****/

/**
 * zkUA_addNodeJSONPack:
 * Encodes a node being added/modified into JSON.
 */
void zkUA_addNodeJsonPack(char *nodePath, int nodeClassInt,
        const UA_NodeId requestedNewNodeId, const UA_NodeId parent,
        const UA_NodeId referenceTypeId, void *attr, json_t *nodePack);

/**
 * zkUA_UA_Server_replicateNode:
 * General function to encode a node into JSON and push it to zookeeper.
 * The encoding is done using zkUA_addNodeJSONPack.
 * Creates the path on zookeeper if the znode doesn't exist or simply sets the
 * data to the newly encoded info if it does exist.
 */
void zkUA_UA_Server_replicateNode(int nodeClass,
        const UA_NodeId requestedNewNodeId, const UA_NodeId parentNodeId,
        const UA_NodeId referenceTypeId, void * attr);

/**
 * zkUA_initReadAttributes_server:
 * Calls readAttribute functions for all of the attributes of a given node based on
 * the node's nodeClass. It initializes and fills a given attribute pointer with the
 * results of the multiple read operations.
 */
UA_StatusCode zkUA_initReadAttributes_server(UA_NodeClass *nodeClass,
        UA_Server *server, const UA_NodeId readNode, void **attr);

/**
 * zkUA_findParent:
 * Disabled function - Initially designed to avoid the bruteforce method for
 * locating a node's parent.
 * Its purpose is to browse a node in the inverse direction to find the node's parent.
 */
//void zkUA_findParent(UA_BrowseResult *bResp, UA_NodeId *parentNodeId);

/**
 * zkUA_findParent_recursiveBrowse:
 * The function recursively browses the entire namespace starting with the root object
 * to locate a node's parent node.
 * It compares a node's children with the node to be located to find the parent node.
 */
UA_StatusCode zkUA_findParent_recursiveBrowse(UA_NodeId childId,
        UA_Boolean isInverse, UA_NodeId referenceTypeId, void *handle);

/** zkUA_locateParent;
 * Given a node Id, this function calls the zkUA_findParent_recursiveBrowse function until it
 * finds a node's parent (if it exists in the ns
 */
void zkUA_UA_Server_locateParent(UA_NodeId nodeId, void **locateParent);

/**
 * zkUA_freeAttributes:
 * The function frees the attributes of a node based on its nodeclass.
 */
void zkUA_freeAttributes(UA_NodeClass *nodeClass, void **attributes);

/**
 * Before a node can be re-encoded into JSON and pushed to ZooKeeper some information which is
 * not always available from the intercepted function is required. This includes locating the node's
 * parent node, getting its nodeClass, and acquiring all of its attributes.
 * This helper function acquires all of this information before zkUA_UA_Server_replicateNode is called.
 * Todo: The acquisition of all of the node's references is currently disabled.
 */
UA_StatusCode zkUA_UA_Server_writeAttribute_prepareReplication(
        UA_Server *server, UA_NodeId nodeId);
