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

/* New struct to store the nodeID and browsePath (on zk) of a node */
typedef struct zkUA_NodeId {
    UA_NodeId *node;
    char *browsePath;
} zkUA_NodeId;

/**
 * zkUA_initRecursive:
 * Initializes variables needed for the recursive browsing of an OPC UA Server
 * by a zkUA Client.
 */
void zkUA_initRecursive(UA_Client *UAclient);

/**
 * zkUA_checkduplicate:
 * Diagnostics function to ensure that the recursive browse function
 * (zkUA_BrowseFolder_recursive) does not browse a node in the address
 * space of an OPC UA Server more than once.
 */
void zkUA_checkduplicate();

/**
 * zkUA_ReadAttributes:
 * This function reads all of the attributes for a given node on
 * an OPC UA Server.
 * The attributes read depend on the node's nodeClass.
 * The results of read attributes are stored in the nodeClass's attributes
 * struct. This struct is provided by the calling function.
 */
void zkUA_ReadAttributes(UA_Client *UAclient, UA_ReferenceDescription *childRef,
        void *attributes);

/**
 * zkUA_BrowseFolder:
 * Uses open62541 code from examples/client.c as a starting point.
 * The code browses a node and prints out the results if it has children
 * If the node has no children it remains silent.
 * Returns the browse response.
 */
void zkUA_BrowseFolder(UA_Client *client, UA_NodeId *browseNode,
        UA_BrowseResponse *bResp);

/**
 * Checks to see if the provided numeric reference identifier
 * represents a hierarchical reference.
 */
int zkUA_hierarchicalReference(int identifierNumeric);

/**
 * zkUA_BrowseFolder_recursive:
 * Recursively browses down a tree (forward direction only),
 * following hierarchical references only, until it exhausts the entire graph
 * (basically it's a depth first search algorithm for an OPC UA Server's
 * address space).
 * The function encodes every unique node browsed with its attributes into a JSON file.
 * The resulting JSON file is pushed to the ZooKeeper-stored address space.
 */
UA_StatusCode
zkUA_BrowseFolder_recursive(UA_NodeId childId, UA_Boolean isInverse,
        UA_NodeId referenceTypeId, void *handle);

/**
 * zkUA_UAServerAddressSpace:
 * Initiates the recursive browsing and replication of the address space of an OPC UA server.
 */
void zkUA_UAServerAddressSpace(zhandle_t *zh, UA_Client *client,
        char *serverAddress, char *groupGuid);
