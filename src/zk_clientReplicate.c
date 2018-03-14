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
#include <zk_clientReplicate.h>
#include <stdio.h>
#include <assert.h>
#include <simple_parse.h>
#include <stdlib.h>
#include <zk_cli.h>
#include <zk_global.h>
#include <jansson.h>
/* currently limiting the number of nodes that can be recursively browsed to 10000 */
int visitedCntr = 0, n = 300, visitedNodeID[300];
UA_Client *client = NULL;
/* The server path on zk */
char *zkServerPath;
zhandle_t *zkHandle; // The zk server's handle;
size_t id = 70000;

/* Initializes variables needed for a client to recursively browse an OPC UA Server */
void zkUA_initRecursive(UA_Client *UAclient) {
    /* TODO: malloc/realloc instead of having a static array for node IDs */
    /* initialize visitedNodeID */
    int j;
    for (j = 0; j < n; j++)
        visitedNodeID[j] = 0;
    /* initialize client var */
    client = UAclient;
}

/* Checks to see if we've browsed the same node ID twice */
void zkUA_checkduplicate() {
    int j, k;
    for (j = 0; j < visitedCntr; j++) {
        for (k = 0; k < visitedCntr; k++) {
            if (j != k) {
                if ((visitedNodeID[j] == visitedNodeID[k])
                        && (visitedNodeID[j] != 0))
                    fprintf(stderr,
                            "This Node ID has been visited twice: %d <-> %d\n",
                            visitedNodeID[j], visitedNodeID[k]);
            }
        }
    }
}

/**
 * zkUA_ReadAttributes:
 * This function reads all of the attributes of a given node.
 * The functions called are dependent on the node's nodeClass.
 * Retrieved attributes are stored in the nodeClass's attributes
 * struct, which is provided by the calling function.
 */
void zkUA_ReadAttributes(UA_Client *UAclient, UA_ReferenceDescription *childRef,
        void *attributes) {

    /* Based on UA_Client_NamespaceGetIndex function */
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    /* Create a UA_NodeId and fill it in from childRef */
    UA_NodeId readNode = childRef->nodeId.nodeId;

    /* Fill provided node based on the node class */
    UA_NodeClass nodeClass = childRef->nodeClass;
    switch (nodeClass) {
    case UA_NODECLASS_VIEW: {
        /* The attributes for a view node. */
        UA_ViewAttributes *viewAttributes = (UA_ViewAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &viewAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &viewAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &viewAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &viewAttributes->userWriteMask);
        UA_Client_readContainsNoLoopsAttribute(UAclient, readNode,
                &viewAttributes->containsNoLoops);
        UA_Client_readEventNotifierAttribute(UAclient, readNode,
                &viewAttributes->eventNotifier);
        break;
    }
    case UA_NODECLASS_VARIABLE: {
        /* Variable has a little more than VariableType
         so if this matches, could just move along afterwards to the VariableType case */
        UA_VariableAttributes *varAttributes =
                (UA_VariableAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &varAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &varAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &varAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &varAttributes->userWriteMask);
        UA_Client_readValueAttribute(UAclient, readNode, &varAttributes->value);
        UA_Client_readDataTypeAttribute(UAclient, readNode,
                &varAttributes->dataType);
        UA_Client_readValueRankAttribute(UAclient, readNode,
                &varAttributes->valueRank);
        UA_Client_readArrayDimensionsAttribute(UAclient, readNode,
                (UA_Int32 **) &varAttributes->arrayDimensions,
                &varAttributes->arrayDimensionsSize);
        UA_Client_readAccessLevelAttribute(UAclient, readNode,
                &varAttributes->accessLevel);
        UA_Client_readUserAccessLevelAttribute(UAclient, readNode,
                &varAttributes->userAccessLevel);
        UA_Client_readMinimumSamplingIntervalAttribute(UAclient, readNode,
                &varAttributes->minimumSamplingInterval);
        UA_Client_readHistorizingAttribute(UAclient, readNode,
                &varAttributes->historizing);
        break;
    }
    case UA_NODECLASS_VARIABLETYPE: {
        UA_VariableTypeAttributes *varTypeAttributes =
                (UA_VariableTypeAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &varTypeAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &varTypeAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &varTypeAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &varTypeAttributes->userWriteMask);
        UA_Client_readValueAttribute(UAclient, readNode,
                &varTypeAttributes->value);
        UA_Client_readDataTypeAttribute(UAclient, readNode,
                &varTypeAttributes->dataType);
        UA_Client_readValueRankAttribute(UAclient, readNode,
                &varTypeAttributes->valueRank);
        UA_Client_readArrayDimensionsAttribute(UAclient, readNode,
                (UA_Int32 **) &varTypeAttributes->arrayDimensions,
                &varTypeAttributes->arrayDimensionsSize);
        UA_Client_readIsAbstractAttribute(UAclient, readNode,
                &varTypeAttributes->isAbstract);
        break;
    }
    case UA_NODECLASS_REFERENCETYPE: {
        UA_ReferenceTypeAttributes *refTypeAttributes =
                (UA_ReferenceTypeAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &refTypeAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &refTypeAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &refTypeAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &refTypeAttributes->userWriteMask);
        UA_Client_readIsAbstractAttribute(UAclient, readNode,
                &refTypeAttributes->isAbstract);
        UA_Client_readSymmetricAttribute(UAclient, readNode,
                &refTypeAttributes->symmetric);
        UA_Client_readInverseNameAttribute(UAclient, readNode,
                &refTypeAttributes->inverseName);
        break;
    }
    case UA_NODECLASS_OBJECTTYPE:
    case UA_NODECLASS_DATATYPE: {
        /* ObjectType and DataType nodes have the same attributes - should use a struct union instead.*/
        UA_DataTypeAttributes *dataTypeAttributes =
                (UA_DataTypeAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &dataTypeAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &dataTypeAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &dataTypeAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &dataTypeAttributes->userWriteMask);
        UA_Client_readIsAbstractAttribute(UAclient, readNode,
                &dataTypeAttributes->isAbstract);
        break;
    }
    case UA_NODECLASS_METHOD: {
        UA_MethodAttributes *methodAttributes =
                (UA_MethodAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &methodAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &methodAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &methodAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &methodAttributes->userWriteMask);
        UA_Client_readExecutableAttribute(UAclient, readNode,
                &methodAttributes->executable);
        UA_Client_readUserExecutableAttribute(UAclient, readNode,
                &methodAttributes->userExecutable);
        break;
    }
    case UA_NODECLASS_OBJECT: {
        UA_ObjectAttributes *objectAttributes =
                (UA_ObjectAttributes *) attributes;
        UA_Client_readDisplayNameAttribute(UAclient, readNode,
                &objectAttributes->displayName);
        UA_Client_readDescriptionAttribute(UAclient, readNode,
                &objectAttributes->description);
        UA_Client_readWriteMaskAttribute(UAclient, readNode,
                &objectAttributes->writeMask);
        UA_Client_readUserWriteMaskAttribute(UAclient, readNode,
                &objectAttributes->userWriteMask);
        UA_Client_readEventNotifierAttribute(UAclient, readNode,
                &objectAttributes->eventNotifier);
        break;
    }
    case UA_NODECLASS_UNSPECIFIED:
    default:
        fprintf(stderr, "zkUA_ReadAttributes: Bad nodeClass error!\n");
        break;
    }
}

/* Browse a node and print out the results. */
void zkUA_BrowseFolder(UA_Client *UAclient, UA_NodeId *browseNode,
        UA_BrowseResponse *bResp) {
    /* Browse some objects */
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = *browseNode; /* browse folder of 'browseNode' */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    *bResp = UA_Client_Service_browse(UAclient, bReq);

    /* Proceed if the browse showed the node has children*/
    if (bResp->results[0].referencesSize > 0) {
        /* print out the results */
        printf("%-9s %-16s %-16s %-16s\n", "NAMESPACE", "NODEID", "BROWSE NAME",
                "DISPLAY NAME");
        for (size_t k = 0; k < bResp->resultsSize; ++k) {
            for (size_t j = 0; j < bResp->results[k].referencesSize; ++j) {
                UA_ReferenceDescription *ref =
                        &(bResp->results[k].references[j]);
                if (ref->nodeId.nodeId.identifierType
                        == UA_NODEIDTYPE_NUMERIC) {
                    printf("%-9d %-16d %-16.*s %-16.*s\n",
                            ref->browseName.namespaceIndex,
                            ref->nodeId.nodeId.identifier.numeric,
                            (int) ref->browseName.name.length,
                            ref->browseName.name.data,
                            (int) ref->displayName.text.length,
                            ref->displayName.text.data);
                } else if (ref->nodeId.nodeId.identifierType
                        == UA_NODEIDTYPE_STRING) {
                    printf("%-9d %-16.*s %-16.*s %-16.*s\n",
                            ref->browseName.namespaceIndex,
                            (int) ref->nodeId.nodeId.identifier.string.length,
                            ref->nodeId.nodeId.identifier.string.data,
                            (int) ref->browseName.name.length,
                            ref->browseName.name.data,
                            (int) ref->displayName.text.length,
                            ref->displayName.text.data);
                }
                /* TODO: distinguish further types */
                else if (ref->nodeId.nodeId.identifierType
                        == UA_NODEIDTYPE_GUID) {
                    printf("Node ID Type GUID\n");
                } else if (ref->nodeId.nodeId.identifierType
                        == UA_NODEIDTYPE_BYTESTRING) {
                    printf("Node ID Type ByteString\n");
                }
            }
        }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
}

/* Checks to see if the provided identifier is that of a hierarchical reference. */
int zkUA_hierarchicalReference(int identifierNumeric) {

    switch (identifierNumeric) {
    case UA_NS0ID_HASCHILD:
    case UA_NS0ID_AGGREGATES:
    case UA_NS0ID_ORGANIZES:
    case UA_NS0ID_HASCOMPONENT:
    case UA_NS0ID_HASORDEREDCOMPONENT:
    case UA_NS0ID_HASPROPERTY:
    case UA_NS0ID_HASSUBTYPE:
    case UA_NS0ID_HASEVENTSOURCE:
    case UA_NS0ID_HASNOTIFIER:
    case UA_NS0ID_HASHISTORICALCONFIGURATION:
        return 1;
    default:
        return 0;
    }

}

/* Recursively browse an OPC UA address space, encode unqiue nodes into JSON
 * and push to ZooKeeper.
 */
UA_StatusCode zkUA_BrowseFolder_recursive(UA_NodeId childId,
        UA_Boolean isInverse, UA_NodeId referenceTypeId, void *handle) {
    /* Browse in the inverse direction */
    if (isInverse) {
        return UA_STATUSCODE_GOOD;
    }

    zkUA_NodeId *zkparent = (zkUA_NodeId *) handle;
    UA_NodeId *parent = zkparent->node;
    printf("%d, %d --- %d ---> NodeId %d, %d\n", parent->namespaceIndex,
            parent->identifier.numeric, referenceTypeId.identifier.numeric,
            childId.namespaceIndex, childId.identifier.numeric);

    /* Browse/push its children only if we've never seen this node before */
    for (size_t j = 0; j < n; j++) {
        if (visitedNodeID[j] == childId.identifier.numeric) {
            return UA_STATUSCODE_GOOD;
        }
    }
    /* Browse parent to get this child's name */
    UA_BrowseResponse bResp_parent;
    zkUA_BrowseFolder(client, parent, &bResp_parent);
    if (bResp_parent.responseHeader.serviceResult != UA_STATUSCODE_GOOD)
        printf("bResp_parent for Node Id %d was not GOOD\n",
                parent->identifier.numeric);
    else
        printf("bResp_parent returned resultsSize = %lu\n",
                bResp_parent.resultsSize);

    /* Allocate space for the child's browse path and REST URI*/
    char *zkChildBrowsePath = (char *) calloc(65535, sizeof(char));
    char * zkChildRestPath = (char *) calloc(65535, sizeof(char));
    /* Find the child's browse name in the response */
    for (size_t k = 0; k < bResp_parent.resultsSize; ++k) {
        for (size_t j = 0; j < bResp_parent.results[k].referencesSize; ++j) {
            UA_ReferenceDescription *child_ref =
                    &(bResp_parent.results[k].references[j]);
            /* TODO: handle the various types of nodeID identifiers */
            if (child_ref->nodeId.nodeId.identifierType
                    == UA_NODEIDTYPE_NUMERIC) {
                if (child_ref->nodeId.nodeId.identifier.numeric
                        == childId.identifier.numeric) {
                    /* create the browse path of the child
                     fill it with the child's (not child's child's) browse path */
                    snprintf(zkChildBrowsePath, 65535, "%s/%.*s",
                            zkparent->browsePath,
                            (int) child_ref->browseName.name.length,
                            child_ref->browseName.name.data);
                    fprintf(stderr, "The browse path is %s\n",
                            zkChildBrowsePath);
                    /* Not the best way around things but... correct the size of the browse path free'ing memory */
                    zkChildBrowsePath = realloc(zkChildBrowsePath,
                            (strlen(zkChildBrowsePath) + 1) * sizeof(char));
                    /* Initialize path for the node in the form
                     * serverAddress/ns=namespaceIndex;nodeIdType=nodeId
                     * Inspired by Issue 99 of open62541 */
                    char *RESTbuffer = (char *) calloc(65535, sizeof(char));
                    snprintf(RESTbuffer, 65535, "ns=%d;i=%d",
                            child_ref->nodeId.nodeId.namespaceIndex,
                            child_ref->nodeId.nodeId.identifier.numeric);
                    snprintf(zkChildRestPath, 65535, "%s/%s", zkServerPath,
                            RESTbuffer);
                    free(RESTbuffer);
                    zkChildRestPath = realloc(zkChildRestPath,
                            (strlen(zkChildRestPath) + 1) * sizeof(char));
                    fprintf(stderr, "The RESTful path is %s\n",
                            zkChildRestPath);
                    /* Get the attributes of the node */
                    /* Initialize the json object that will hold the encoded attributes */
                    json_t *jsonAttributes = json_object();
                    /* This should work as long as the nodeClass is not UA_NODECLASS_UNSPECIFIED */
                    switch (child_ref->nodeClass) {
                    case UA_NODECLASS_OBJECT: {
                        UA_ObjectAttributes nodeAttributes;
                        UA_ObjectAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_ObjectAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_METHOD: {
                        UA_MethodAttributes nodeAttributes;
                        UA_MethodAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_MethodAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_VIEW: {
                        UA_ViewAttributes nodeAttributes;
                        UA_ViewAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_ViewAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_VARIABLE: {
                        UA_VariableAttributes nodeAttributes;
                        UA_VariableAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_VariableAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_VARIABLETYPE: {
                        UA_VariableTypeAttributes nodeAttributes;
                        UA_VariableTypeAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_VariableTypeAttributes_deleteMembers(
                                &nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_REFERENCETYPE: {
                        UA_ReferenceTypeAttributes nodeAttributes;
                        UA_ReferenceTypeAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_ReferenceTypeAttributes_deleteMembers(
                                &nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_OBJECTTYPE: {
                        UA_ObjectTypeAttributes nodeAttributes;
                        UA_ObjectTypeAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_ObjectTypeAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_DATATYPE: {
                        UA_DataTypeAttributes nodeAttributes;
                        UA_DataTypeAttributes_init(&nodeAttributes);
                        zkUA_ReadAttributes(client, child_ref, &nodeAttributes);
                        zkUA_jsonEncodeAttributes(child_ref, &nodeAttributes,
                                jsonAttributes);
                        UA_DataTypeAttributes_deleteMembers(&nodeAttributes);
                        break;
                    }
                    case UA_NODECLASS_UNSPECIFIED:
                    default:
                        printf("nodeAttributes not initialized");
                        break;
                    }
                    /* Package all of the attributes and references for zookeeper */
                    json_t *nodePack = json_object();
                    json_t *nodeId = json_object();
                    zkUA_jsonEncode_UA_NodeId(&childId, nodeId);
                    json_t *nodeClass = json_integer(child_ref->nodeClass);
                    json_t *browsePath = json_string(zkChildBrowsePath);
                    json_t *restPath = json_string(zkChildRestPath);

                    json_t *parentNodeId = json_object();
                    zkUA_jsonEncode_UA_NodeId(parent, parentNodeId);
                    json_t *parentReferenceNodeId = json_object();
                    zkUA_jsonEncode_UA_NodeId(&referenceTypeId,
                            parentReferenceNodeId);

                    json_t *nodeInfo = json_object();

                    json_object_set_new(nodeInfo, "NodeId", nodeId);
                    json_object_set_new(nodeInfo, "NodeClass", nodeClass);
                    json_object_set_new(nodeInfo, "parentNodeId", parentNodeId);
                    json_object_set_new(nodeInfo, "parentReferenceNodeId",
                            parentReferenceNodeId);
                    json_object_set_new(nodeInfo, "BrowsePath", browsePath);
                    json_object_set_new(nodeInfo, "restPath", restPath);

                    json_object_set_new(nodePack, "NodeInfo", nodeInfo);
                    json_object_set_new(nodePack, "Attributes", jsonAttributes);
                    /* push the browse path to zk with its attributes and references*/
                    char *s = json_dumps(nodePack, JSON_INDENT(1));
                    if (!s) {
                        fprintf(stderr, "json_dumps failed\n");
                    } else {
                        int flags = 0;
                        int rc = zoo_acreate(zkHandle, zkChildRestPath, s,
                                strlen(s), &ZOO_OPEN_ACL_UNSAFE, flags,
                                zkUA_my_string_completion_free_data,
                                strdup(zkChildRestPath));
                        if (rc) {
                            fprintf(stderr, "Error %d for %s\n", rc,
                                    zkChildRestPath);
                        } else {
                        }
                    }
                    /* If push is successful -> this node has now been visited */
                    /* TODO: visitedNodeId array should hold namespaceIndex and identifier (multiple types) */
                    visitedNodeID[visitedCntr++] = childId.identifier.numeric;
                    /* memory clean up */
                    free(s);
                    json_decref(nodePack);
                }
            } else if (child_ref->nodeId.nodeId.identifierType
                    == UA_NODEIDTYPE_STRING) {
                printf(
                        "The nodeId.identifierType for %s/%.*s is a string type\n",
                        zkparent->browsePath,
                        (int) child_ref->browseName.name.length,
                        child_ref->browseName.name.data);
            }
        }
    }
    /* We're done, call the recursive function to browse this node's children */
    UA_NodeId *parentNew = UA_NodeId_new();
    /* Create a UA_NodeId struct for the child */
    *parentNew = UA_NODEID_NUMERIC(childId.namespaceIndex,
            childId.identifier.numeric);
    /* Create a struct to hold the UA_NodeId and the browse path of the child */
    zkUA_NodeId *parentNew_zk = (zkUA_NodeId *) calloc(1, sizeof(zkUA_NodeId));
    parentNew_zk->node = parentNew;
    parentNew_zk->browsePath = zkChildBrowsePath;
    /* Call this function for each of the child's children */
    UA_Client_forEachChildNodeCall(client, *parentNew,
            zkUA_BrowseFolder_recursive, (void *) parentNew_zk);
    /* Free the memory assigned to the child */
    UA_BrowseResponse_deleteMembers(&bResp_parent);
    UA_NodeId_delete(parentNew);
    free(zkChildBrowsePath);
    free(zkChildRestPath);
    free(parentNew_zk);
    return UA_STATUSCODE_GOOD;
}

void zkUA_UAServerAddressSpace(zhandle_t *zh, UA_Client *client,
        char *serverAddress, char *groupGuid) {
    zkHandle = zh;
    /* Initialize zkServerAddressSpacePath string and the path on zookeeper */
    zkUA_initializeZkServAddSpacePath(groupGuid, zh);
    /* Call a recursive browse starting from the zkparent node */
    /* Recursively browse the server's address space */
    zkUA_initRecursive(client);

    zkUA_NodeId *zkparent = (zkUA_NodeId *) malloc(sizeof(zkUA_NodeId));
    UA_NodeId *parent = UA_NodeId_new();
    /* If we are at the root then the parent ID is the root ID */
    *parent = UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER);
    /* Initialize the zk child path with the server's root path */
    zkparent->node = parent;
    zkparent->browsePath = strdup(zkUA_zkServAddSpacePath());
    zkServerPath = strdup(zkUA_zkServAddSpacePath());
    /* Call a recursive browse starting from the zkparent node */
    UA_Client_forEachChildNodeCall(client,
            UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER),
            zkUA_BrowseFolder_recursive, (void *) zkparent);
    /* free memory and return */
    UA_NodeId_delete(parent);
    free(zkparent->browsePath);
    free(zkServerPath);
    free(zkparent);

}
