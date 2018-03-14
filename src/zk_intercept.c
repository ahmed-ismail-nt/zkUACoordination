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
#include <zk_serverReplicate.h>
#include <zk_clientReplicate.h>
#include <zk_cli.h>
#include <zk_global.h>
/* Debugging */
#include <simple_parse.h>

UA_Server *uaServer;
int visitedCntrServer = 0, nServer = 3000, visitedNodeIDServer[3000];

/* Intercepts calls to UA_Server_deleteNode to check if the deletion should be replicated to ZooKeeper. */
UA_StatusCode zkUA_Service_DeleteNodes_single(UA_Server *server,
        UA_Session *session, const UA_NodeId *nodeId,
        UA_Boolean deleteReferences) {

    fprintf(stderr,
            "zkUA_UA_Server_deleteNode: zk_intercept.c: Intercepted call to UA_Server_deleteNode\n");
    int rc = 1;
    if (replicateNode == true) {
        /* TODO: atomically delete the node -
         * i.e. if one fails, roll back deletion*/
        /* delete the node on zookeeper */
        char *fullNodePath = zkUA_encodeZnodePath(nodeId);
        /* Doesn't matter - if it doesn't exist we won't be able to delete it */
        rc = zoo_delete(zkHandle, fullNodePath, -1);
        if (rc) {
            fprintf(stderr, "Error %d for %s\n", rc, fullNodePath);
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    }
    /* delete the node in the namespace */
    return _Service_DeleteNodes_single(server, session, nodeId,
            deleteReferences);
}
/* Deletes an OPC UA node from the local cache only */
UA_StatusCode zkUA_UA_Server_deleteNode_dontReplicate(UA_Server *server,
        const UA_NodeId nodeId, UA_Boolean deleteReferences) {
    fprintf(stderr,
            "zkUA_Server_deleteNode_dontReplicate: Deleting ns=%d;i=%d\n",
            nodeId.namespaceIndex, nodeId.identifier.numeric);
    replicateNode = false;
    return UA_Server_deleteNode(server, nodeId, deleteReferences);
    replicateNode = true;
}

/* Encodes a node being added/modified into JSON */
void zkUA_addNodeJsonPack(char *nodePath, int nodeClassInt,
        const UA_NodeId requestedNewNodeId, const UA_NodeId parent,
        const UA_NodeId referenceTypeId, void *attr, json_t *nodePack) {

    /* Encode the node attributes*/
    UA_ReferenceDescription *nodeRef = UA_ReferenceDescription_new();
    nodeRef->nodeClass = nodeClassInt;
    json_t *jsonAttributes = json_object();
    zkUA_jsonEncodeAttributes(nodeRef, attr, jsonAttributes);
    UA_ReferenceDescription_deleteMembers(nodeRef);
    free(nodeRef);
    /* Encode the nodeInfo */
    json_t *jsonNodeId = json_object();

    fprintf(stderr, "zkUA_addNodeJsonPack: ns=%d id=%u idtype=%d\n",
            requestedNewNodeId.namespaceIndex,
            requestedNewNodeId.identifier.numeric,
            requestedNewNodeId.identifierType);
    zkUA_jsonEncode_UA_NodeId((UA_NodeId *) &requestedNewNodeId, jsonNodeId);
    json_t *nodeClass = json_integer(nodeClassInt);
    json_t *restPath = json_string(nodePath);

    json_t *parentNodeId = json_object();
    zkUA_jsonEncode_UA_NodeId((UA_NodeId *) &parent, parentNodeId);
    json_t *parentReferenceNodeId = json_object();
    zkUA_jsonEncode_UA_NodeId((UA_NodeId *) &referenceTypeId,
            parentReferenceNodeId);

    json_t *nodeInfo = json_object();

    json_object_set_new(nodeInfo, "NodeId", jsonNodeId);
    json_object_set_new(nodeInfo, "NodeClass", nodeClass);
    json_object_set_new(nodeInfo, "parentNodeId", parentNodeId);
    json_object_set_new(nodeInfo, "parentReferenceNodeId",
            parentReferenceNodeId);
    json_object_set_new(nodeInfo, "restPath", restPath);

    json_object_set_new(nodePack, "NodeInfo", nodeInfo);
    json_object_set_new(nodePack, "Attributes", jsonAttributes);
    return;
}

/**
 * zkUA_UA_Server_replicateNode:
 * General function to encode a node into JSON and push it to zookeeper.
 * Creates the path on zookeeper if the znode doesn't exist or simply sets the
 * data to the newly encoded info if it does exist.
 */
void zkUA_UA_Server_replicateNode(int nodeClass,
        const UA_NodeId requestedNewNodeId, const UA_NodeId parentNodeId,
        const UA_NodeId referenceTypeId, void * attr) {

    int rc = -1;
    /* TODO: atomically delete the node - if one fails, rollback */
    /* initialize the zookeeper node path */
    char *nodePath = zkUA_encodeZnodePath(&requestedNewNodeId);
    /* Initialize the JSON root object */
    json_t *nodePack = json_object();
    zkUA_addNodeJsonPack(nodePath, nodeClass, requestedNewNodeId, parentNodeId,
            referenceTypeId, (void *) attr, nodePack);
    /* Encode the node attributes*/
    char *s = json_dumps(nodePack, JSON_INDENT(1));
    if (s == NULL)
        fprintf(stderr,
                "zkUA_UA_Server_replicateNode: Could not dump nodePack\n");
    json_decref(nodePack);
    /* Check if the path exists on zookeeper */
    struct Stat stat;
    if (zkHandle) {
        fprintf(stderr, "zkUA_UA_Server_replicateNode: Checking if %s exists\n",
                nodePath);
        rc = zoo_exists(zkHandle, nodePath, 0, &stat);
    }
    /* If it does, set the data for the same node */
    if (rc == ZOK) {
        fprintf(stderr,
                "zkUA_UA_Server_replicateNode: Path %s exists. Setting nodePath data with new value\n",
                nodePath);
        rc = zoo_set2(zkHandle, nodePath, s, strlen(s), -1, &stat);
        /* Update the hashtable mzxid */
        zkUA_insertMzxidAge(nodePath, (long long *) &stat.mzxid);
    } else if (rc == ZNONODE) { /* If it doesn't exist */
        /* create the path with the encoded data */
        fprintf(stderr,
                "zkUA_UA_Server_replicateNode: Path %s doesn't exist. Creating nodePath and setting data\n",
                nodePath);
        int flags = 0;
        char *path_buffer = calloc(65535, sizeof(char));
        rc = zoo_create(zkHandle, nodePath, s, strlen(s), &ZOO_OPEN_ACL_UNSAFE,
                flags, path_buffer, 65535);
        /* get the node to acquire the stat & so acquire the mzxid */
        int pathBufferLen;
        rc = zoo_get(zkHandle, nodePath, 0, path_buffer, &pathBufferLen, &stat);
        free(path_buffer);
        /* Update the hashtable mzxid */
        zkUA_insertMzxidAge(nodePath, (long long *) &stat.mzxid);
    } else {
        fprintf(stderr,
                "zkUA_UA_Server_replicateNode: Could not add the node to zk or set its data - rc = %d\n",
                rc);
    }
    free(nodePath);
    free(s);
}

UA_StatusCode zkUA_initReadAttributes_server(UA_NodeClass *nodeClass,
        UA_Server *server, const UA_NodeId readNode, void **attr) {

    switch (*nodeClass) {
    case (UA_NODECLASS_OBJECT): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading object attributes\n");
        UA_ObjectAttributes *objectAttributes = UA_ObjectAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &objectAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &objectAttributes->description);
        UA_Server_readWriteMask(server, readNode, &objectAttributes->writeMask);
        UA_Server_readEventNotifier(server, readNode,
                &objectAttributes->eventNotifier);
        *attr = objectAttributes;
        break;
    }
    case (UA_NODECLASS_VARIABLE): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading variable attributes\n");
        UA_VariableAttributes *varAttributes = UA_VariableAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &varAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &varAttributes->description);
        UA_Server_readWriteMask(server, readNode, &varAttributes->writeMask);
        UA_Server_readValue(server, readNode, &varAttributes->value);
        UA_Server_readDataType(server, readNode, &varAttributes->dataType);
        UA_Server_readValueRank(server, readNode, &varAttributes->valueRank);
        UA_Variant *newVal = UA_Variant_new();
        UA_Server_readArrayDimensions(server, readNode, newVal); /* returns a variant with an int32 array */
        varAttributes->arrayDimensions = newVal->arrayDimensions;
        varAttributes->arrayDimensionsSize = newVal->arrayDimensionsSize;
        UA_Variant_deleteMembers(newVal);
        free(newVal);
        UA_Server_readAccessLevel(server, readNode,
                &varAttributes->accessLevel);
        UA_Server_readMinimumSamplingInterval(server, readNode,
                &varAttributes->minimumSamplingInterval);
        UA_Server_readHistorizing(server, readNode,
                &varAttributes->historizing);
        *attr = varAttributes;
        break;
    }
    case (UA_NODECLASS_VARIABLETYPE): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading variableType attributes\n");
        UA_VariableTypeAttributes *varTypeAttributes =
                UA_VariableTypeAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &varTypeAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &varTypeAttributes->description);
        UA_Server_readWriteMask(server, readNode,
                &varTypeAttributes->writeMask);
        UA_Server_readValue(server, readNode, &varTypeAttributes->value);
        UA_Server_readDataType(server, readNode, &varTypeAttributes->dataType);
        UA_Server_readValueRank(server, readNode,
                &varTypeAttributes->valueRank);
        UA_Variant *newVal = UA_Variant_new();
        UA_Server_readArrayDimensions(server, readNode, newVal); /* returns a variant with an int32 array */
        varTypeAttributes->arrayDimensions = newVal->arrayDimensions;
        varTypeAttributes->arrayDimensionsSize = newVal->arrayDimensionsSize;
        UA_Variant_deleteMembers(newVal);
        free(newVal);
        UA_Server_readIsAbstract(server, readNode,
                &varTypeAttributes->isAbstract);
        *attr = varTypeAttributes;
        break;
    }
    case (UA_NODECLASS_METHOD): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading Method attributes\n");
        UA_MethodAttributes *methodAttributes = UA_MethodAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &methodAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &methodAttributes->description);
        UA_Server_readWriteMask(server, readNode, &methodAttributes->writeMask);
        UA_Server_readExecutable(server, readNode,
                &methodAttributes->executable);
        *attr = methodAttributes;
        break;
    }
    case (UA_NODECLASS_OBJECTTYPE): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading objectType attributes\n");
        UA_ObjectTypeAttributes *objectTypeAttributes =
                UA_ObjectTypeAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &objectTypeAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &objectTypeAttributes->description);
        UA_Server_readWriteMask(server, readNode, &objectTypeAttributes->writeMask);
        UA_Server_readIsAbstract(server, readNode,
                &objectTypeAttributes->isAbstract);
        *attr = objectTypeAttributes;
        break;
    }
    case (UA_NODECLASS_DATATYPE): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading dataType attributes\n");
        UA_DataTypeAttributes *dataTypeAttributes = UA_DataTypeAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &dataTypeAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &dataTypeAttributes->description);
        UA_Server_readWriteMask(server, readNode, &dataTypeAttributes->writeMask);
        UA_Server_readIsAbstract(server, readNode,
                &dataTypeAttributes->isAbstract);
        *attr = dataTypeAttributes;
        break;
    }
    case (UA_NODECLASS_REFERENCETYPE): {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Reading referenceType attributes\n");
        UA_ReferenceTypeAttributes *refTypeAttributes =
                UA_ReferenceTypeAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &refTypeAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &refTypeAttributes->description);
        UA_Server_readWriteMask(server, readNode,
                &refTypeAttributes->writeMask);
        UA_Server_readIsAbstract(server, readNode,
                &refTypeAttributes->isAbstract);
        UA_Server_readSymmetric(server, readNode,
                &refTypeAttributes->symmetric);
        UA_Server_readInverseName(server, readNode,
                &refTypeAttributes->inverseName);
        *attr = refTypeAttributes;
        break;
    }
    case (UA_NODECLASS_VIEW): {
        UA_ViewAttributes *viewAttributes = UA_ViewAttributes_new();
        UA_Server_readDisplayName(server, readNode,
                &viewAttributes->displayName);
        UA_Server_readDescription(server, readNode,
                &viewAttributes->description);
        UA_Server_readWriteMask(server, readNode, &viewAttributes->writeMask);
        UA_Server_readContainsNoLoop(server, readNode,
                &viewAttributes->containsNoLoops); /* client is UA_Client_readContainsNoLoops here it's Loop - submit PR*/
        UA_Server_readEventNotifier(server, readNode,
                &viewAttributes->eventNotifier);
        *attr = viewAttributes;
        break;
    }
    case (UA_NODECLASS_UNSPECIFIED): {
    }
    default: {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Bad nodeClass error!\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
        break;
    }
    }
    return UA_STATUSCODE_GOOD;
}

void zkUA_findParent(UA_BrowseResult *bResp, UA_NodeId *parentNodeId) {

    fprintf(stderr, "zkUA_findParent: referencesSize = %lu\n",
            bResp->referencesSize);
    /* Proceed if the browse showed the node has children*/
    if (bResp->referencesSize > 0) {
        /* print out the results */
        printf("%-9s %-16s %-16s %-16s\n", "NAMESPACE", "NODEID", "BROWSE NAME",
                "DISPLAY NAME");
        for (size_t j = 0; j < bResp->referencesSize; ++j) {
            UA_ReferenceDescription *ref = &(bResp->references[j]);
            if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
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
            else if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_GUID) {
                printf("Node ID Type GUID\n");
            } else if (ref->nodeId.nodeId.identifierType
                    == UA_NODEIDTYPE_BYTESTRING) {
                printf("Node ID Type ByteString\n");
            }
        }
    } else
        return;
    return;
}

UA_StatusCode zkUA_findParent_recursiveBrowse(UA_NodeId childId,
        UA_Boolean isInverse, UA_NodeId referenceTypeId,
        void *handle /*store parent when located here*/) {
    if (isInverse) {
        return UA_STATUSCODE_GOOD;
    }

    /* Browse down this path only if we've never seen this child before */
    /* TODO: visitedNodeId array should hold namespaceIndex and identifier (multiple types) */
    for (size_t j = 0; j < nServer; j++) {
        if (visitedNodeIDServer[j] == childId.identifier.numeric) {
            return UA_STATUSCODE_GOOD;
        }
    }

    zkUA_locateParent *locateParent = (zkUA_locateParent *) handle;
    /* Only accept the parent node for this child if the child is pointed
     to by a hierarchicalReference */
    if (!zkUA_hierarchicalReference(referenceTypeId.identifier.numeric)) {
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    /* TODO: visitedNodeId array should hold namespaceIndex and identifier (multiple types) */
    visitedNodeIDServer[visitedCntrServer++] = childId.identifier.numeric;

    /* If the namespaceIndexes match */
    if (childId.namespaceIndex
            == locateParent->searchedForNode->namespaceIndex) {
        /* TODO: handle other identifier types */
        /* if the nodeIds match */
        if (childId.identifier.numeric
                == locateParent->searchedForNode->identifier.numeric) {
            locateParent->foundParent->identifier.numeric =
                    locateParent->parentNode->identifier.numeric;
            locateParent->foundParent->namespaceIndex =
                    locateParent->parentNode->namespaceIndex;
            *locateParent->referenceTypeId = referenceTypeId;
            locateParent->foundParentFlag = true;
            fprintf(stderr,
                    "zkUA_findParent_recursiveBrowse: Found parent! ns=%d id=%d\n",
                    locateParent->parentNode->namespaceIndex,
                    locateParent->parentNode->identifier.numeric);
            return UA_STATUSCODE_GOOD;
        }
    }
    /* Well, we didn't find the parent yet, so initialize a new struct and go down another depth level */
    zkUA_locateParent *locateParentNew = (zkUA_locateParent *) calloc(1,
            sizeof(zkUA_locateParent));
    UA_NodeId *parent = UA_NodeId_new();
    *parent = childId;
    locateParentNew->parentNode = parent;
    locateParentNew->searchedForNode = locateParent->searchedForNode;
    locateParentNew->foundParent = locateParent->foundParent;
    locateParentNew->referenceTypeId = locateParent->referenceTypeId;
    UA_Server_forEachChildNodeCall(uaServer, childId,
            zkUA_findParent_recursiveBrowse, (void *) locateParentNew);
    /* If we returned from the call, clean up */
    UA_NodeId_deleteMembers(locateParentNew->parentNode);
    free(locateParentNew);
    UA_NodeId_deleteMembers(parent);
    free(parent);
    return UA_STATUSCODE_BADUNEXPECTEDERROR;
}

void zkUA_freeAttributes(UA_NodeClass *nodeClass, void **attributes) {
    switch (*nodeClass) {
    case (UA_NODECLASS_OBJECT): {
        UA_ObjectAttributes_deleteMembers((UA_ObjectAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_VARIABLE): {
        UA_Variant_deleteMembers(
                &((UA_VariableAttributes *) *attributes)->value);
        UA_VariableAttributes_deleteMembers(
                (UA_VariableAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_VARIABLETYPE): {
        UA_VariableTypeAttributes_deleteMembers(
                (UA_VariableTypeAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_METHOD): {
        UA_MethodAttributes_deleteMembers((UA_MethodAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_OBJECTTYPE): {
        fprintf(stderr, "zkUA_freeAttributes: Freeing objectTypeAttributes\n");
        UA_LocalizedText_deleteMembers(
                &((UA_ObjectTypeAttributes*) *attributes)->displayName);
        UA_LocalizedText_deleteMembers(
                &((UA_ObjectTypeAttributes*) *attributes)->description);
        UA_ObjectTypeAttributes_deleteMembers(
                (UA_ObjectTypeAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_DATATYPE): {
        UA_DataTypeAttributes_deleteMembers(
                (UA_DataTypeAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_REFERENCETYPE): {
        UA_ReferenceTypeAttributes_deleteMembers(
                (UA_ReferenceTypeAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_VIEW): {
        UA_ViewAttributes_deleteMembers((UA_ViewAttributes*) *attributes);
        break;
    }
    case (UA_NODECLASS_UNSPECIFIED): {
    }
    default: {
        fprintf(stderr,
                "zkUA_initReadAttributes_server: Bad nodeClass error!\n");
        break;
    }
    }
    free(*attributes);
//    return UA_STATUSCODE_GOOD;
}

void zkUA_UA_Server_locateParent(UA_NodeId nodeId, void **locateParent) {
    for (int i = 0; i < nServer; i++) { /* initialize visited nodes array */
        visitedNodeIDServer[i] = 0;
    }
    zkUA_locateParent *locParent = (zkUA_locateParent *) *locateParent;
    UA_NodeId parent = UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER);
    locParent->parentNode = &parent;
    UA_NodeId *searchedForNode = UA_NodeId_new();
    *searchedForNode = nodeId;
    locParent->searchedForNode = searchedForNode;
    UA_NodeId *foundParent = UA_NodeId_new();
    locParent->foundParent = foundParent;
    locParent->foundParent->namespaceIndex = -1;
    locParent->foundParentFlag = false;
    UA_NodeId *rTypeId = UA_NodeId_new();
    locParent->referenceTypeId = rTypeId;
    fprintf(stderr,
            "zkUA_UA_Server_locateParent: Starting with ns=%d;id=%d - Looking for ns=%d;id=%d\n",
            locParent->parentNode->namespaceIndex,
            locParent->parentNode->identifier.numeric,
            locParent->searchedForNode->namespaceIndex,
            locParent->searchedForNode->identifier.numeric);
    UA_Server_forEachChildNodeCall(uaServer,
            UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER),
            zkUA_findParent_recursiveBrowse, (void *) *locateParent);
}

UA_StatusCode zkUA_UA_Server_writeAttribute_prepareReplication(
        UA_Server *server, UA_NodeId nodeId) {

    void *attributes = NULL;
    uaServer = server;

    /* Create a struct to hold the currently browsed parent node, searched for node,
     and, if located, the located correct parent node */
    zkUA_locateParent *locateParent = (zkUA_locateParent *) calloc(1,
            sizeof(zkUA_locateParent));
    zkUA_UA_Server_locateParent(nodeId, (void **) &locateParent);
    if (locateParent->foundParent->namespaceIndex == -1) {
        fprintf(stderr,
                "zkUA_UA_Server_writeAttribute_prepareReplication: Error! Could not find parent node!\n");
    }
    /* Find the nodeClass */
    UA_NodeClass *nodeClass = UA_NodeClass_new();
    UA_Server_readNodeClass(server, nodeId, nodeClass);
    /* Call function based on nodeClass to read all that nodeClass's attributes */
    zkUA_initReadAttributes_server(nodeClass, server, nodeId, &attributes); // Like zkUA_browsefolder_recursive, this function initializes attributes based on
    // the nodeClass and then calls zkUA_readAttributes_server to fill in attr
    /* Encode into JSON and push to ZooKeeper */
    zkUA_UA_Server_replicateNode(*nodeClass, nodeId, *locateParent->foundParent,
            *locateParent->referenceTypeId, attributes);
    zkUA_freeAttributes(nodeClass, &attributes);
    UA_NodeClass_deleteMembers(nodeClass);
    free(nodeClass);
    UA_NodeId_deleteMembers(locateParent->searchedForNode);
    free(locateParent->searchedForNode);
    UA_NodeId_deleteMembers(locateParent->foundParent);
    free(locateParent->foundParent);
    UA_NodeId_deleteMembers(locateParent->referenceTypeId);
    free(locateParent->referenceTypeId);
    free(locateParent);
    return UA_STATUSCODE_GOOD;
}

/********** Interceptor functions for compilation purposes **********/

/* Intercepting all read requests initiated by the UA Server or UA Client connected to the UA Server */
void zkUA_Service_Read_single(UA_Server *server, UA_Session *session,
        const UA_TimestampsToReturn timestamps, const UA_ReadValueId *id,
        UA_DataValue *v) {

    /* TODO: Since we have watches set on all nodes, we can use the local cache directly
     as the watch mechanism sends out a notification the moment anything changes. */
    fprintf(stderr,
            "zkUA_Service_Read_single: Intercepted call to Service_Read_single\n");

    uaServer = server;
    /* Let's see if we have a running connection with zk ensemble */
    struct Stat stat;
    int rc = zoo_exists(zkHandle, "/", 0, &stat);
    if ((rc == ZOK) || (rc < 0 && availabilityPriority == true)) {
        /* a) If I successfully read from zk then I just updated the local cache with the newest copy
         b) If I failed to read from zk but availability is more important than reliability
         then we read from the local cache */
        _Service_Read_single(server, session, timestamps, id, v);
    }

}

void zkUA_atomicWrite_prepareRollback(UA_Server *server, UA_DataValue *v,
        const UA_WriteValue *value) {

    /* Read the attribute by attribute Id using request->nodesToWrite[ntwsCnt]->attributeId
     and make a new UA_WriteValue to hold the old attribute's value for rollback if failed */
    UA_TimestampsToReturn timestamps = UA_TIMESTAMPSTORETURN_BOTH;
    UA_ReadValueId id;
    id.nodeId = value->nodeId;
    id.attributeId = value->attributeId;
    id.indexRange = value->indexRange;
    /* set dataEncoding to UA_Binary */
    id.dataEncoding.name = (UA_String ) { sizeof("DefaultBinary") - 1,
                    (UA_Byte*) "DefaultBinary" };
    id.dataEncoding.namespaceIndex = 0;
    /* Use UA_Server_read to read the attribute instead of _Service_read_single and you don't
     have to worry about supplying the session info  */
    *v = UA_Server_read(server, (const UA_ReadValueId *) &id, timestamps);
    fprintf(stderr, "zkUA_Service_Write: Read attribute is %d\n",
            id.attributeId);
}

UA_StatusCode zkUA_atomicWrite_initiateRollback(UA_Server *server,
        const UA_WriteValue *value, UA_DataValue *v) {

    UA_WriteValue rollbackWV;
    rollbackWV.nodeId = value->nodeId;
    rollbackWV.attributeId = value->attributeId;
    rollbackWV.indexRange = value->indexRange;
    rollbackWV.value = *v;
    UA_StatusCode sCode = _UA_Server_write(server, &rollbackWV);
    return sCode;
}

/* If a UA Server edits a node's attribute, UA_Server_write is called - since when replicating we
 add the whole node or delete and readd the node which calls Service_AddNodes_single
 when this function is called - always replicate to zk */
UA_StatusCode zkUA_UA_Server_write(UA_Server *server,
        const UA_WriteValue *value) {

    fprintf(stderr,
            "zkUA_UA_Server_write: Intercepted call to UA_Server_write\n");
    uaServer = server;
    /* Make a copy of the node to be modified before modifying it */
    UA_DataValue v;
    zkUA_atomicWrite_prepareRollback(server, &v, value);
    /* Apply the write requests to the local cache */
    UA_StatusCode sCode = _UA_Server_write(server, value);
    /* If write succeeds */
    if (replicateNode == true && sCode == UA_STATUSCODE_GOOD) {
        sCode = zkUA_UA_Server_writeAttribute_prepareReplication(server,
                value->nodeId);
        if (sCode != UA_STATUSCODE_GOOD) { /* rollback changes to local cache */
            fprintf(stderr,
                    "zkUA_Service_Write: Replication to zk failed - rolling back local cache change\n");
            sCode = zkUA_atomicWrite_initiateRollback(server, value, &v);
        }
    }
    return sCode;
}

/* If a UA Client modified a node's attribute, Service write is called - always replicate to zk */
void zkUA_Service_Write(UA_Server *server, UA_Session *session,
        const UA_WriteRequest *request, UA_WriteResponse *response) {
    fprintf(stderr, "zkUA_Service_Write: Intercepted call to Service_write\n");
    uaServer = server;
    size_t ntwsCnt = 0;
    UA_StatusCode sCode = 0;
    /* Initialize an array of pointers to hold the copies of the attributes to be written */
    UA_DataValue v[request->nodesToWriteSize];
    /* Make copies of the nodes to be modified before modifying them */
    for (ntwsCnt = 0; ntwsCnt < request->nodesToWriteSize; ++ntwsCnt) {
        const UA_WriteValue nodeToWrite = request->nodesToWrite[ntwsCnt];
        zkUA_atomicWrite_prepareRollback(server, &v[ntwsCnt], &nodeToWrite);
    }

    /* Apply the write requests to the local cache */
    _Service_Write(server, session, request, response);
    /* For every successful write to the local cache attempt to replicate
     to zk ensemble */
    for (ntwsCnt = 0; ntwsCnt < request->nodesToWriteSize; ++ntwsCnt) {
        if (response->results[ntwsCnt] == UA_STATUSCODE_GOOD) {
            /* only replicate the node to zk if its modification to the local cache succeeded */
            sCode = zkUA_UA_Server_writeAttribute_prepareReplication(server,
                    request->nodesToWrite[ntwsCnt].nodeId);
            /* If the replication failed, rollback the modification done
             to the local cache for that node only */
            if (sCode != UA_STATUSCODE_GOOD) {
                fprintf(stderr,
                        "zkUA_Service_Write: Replication to zk failed - rolling back local cache change for node %lu\n",
                        ntwsCnt);
                UA_WriteValue nodeToWrite = request->nodesToWrite[ntwsCnt];
                sCode = zkUA_atomicWrite_initiateRollback(server, &nodeToWrite,
                        &v[ntwsCnt]);
            }
        }
    }
}

/* Intercepting addnodeinternal */
UA_AddNodesResult zkUA_addNodeInternal(UA_Server *server, UA_Node *node,
        const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId) {
    /* Add node to the internal cache */
    UA_AddNodesResult addNodeResult = _addNodeInternal(server, node,
            parentNodeId, referenceTypeId);
    UA_StatusCode sCode = addNodeResult.statusCode;
    if (sCode == UA_STATUSCODE_GOOD) { /* If the node was added successfully*/
        /* If this is a ns0 node that exists on zk or is a NS0ID_SERVER node or its child
         - don't add because for the former we'll replicate
         for the latter we don't replicate */
        if (node->nodeId.namespaceIndex == 0&&
        node->nodeId.identifier.numeric!=UA_NS0ID_ROOTFOLDER) { /* don't enter if the tree hasn't even been initialized */
            /* check if this ns0 node already exists on zk */
            /*TODO: deduplicate mzxid lookups across the different c files */
            /* Get the  mzxid of the node and see if we have something new(er) */
            char *buffer = calloc(65535, sizeof(char));
            size_t buffer_len = 65535;
            struct Stat stat;
            char *nodeZkPath = zkUA_encodeZnodePath(
                    (const UA_NodeId *) &node->nodeId);
            int rc = zoo_get(zkHandle, nodeZkPath, 1 /* non-zero sets watch */,
                    buffer, (int *) &buffer_len, &stat);
            free(buffer);
            if (rc != ZNONODE && rc != ZOK) { /* We weren't returned stat (i.e.,rc!=ZOK) but we got an error other than no znode exists for that path */
                free(nodeZkPath);
                return addNodeResult;
            }

            /* Prepare values for the hashtable */
            if (rc == ZOK) { /* If the node exists and therefore has an mzxid */
                int mzxidFresher = zkUA_checkMzxidAge(nodeZkPath,
                        ((long long int *) &stat.mzxid));
                free(nodeZkPath);
                /* Let's see if the mzxid for this node path exists in our hashtable or if the retrieved data is fresher */
                if (mzxidFresher <= 0) { /* I have something as old or older than what's on zk */
                    return addNodeResult;
                }
            } /* Otherwise the node doesn't exist or we have something fresher */
            /* Check if this is the NS0ID_SERVER node or part of its subtree */
            free(nodeZkPath);
            if (node->nodeId.identifier.numeric == UA_NS0ID_SERVER)
                return addNodeResult;
            /* check if NS0ID_SERVER node exists */
            UA_BrowseDescription *descr = UA_BrowseDescription_new();
            descr[0].nodeId = (UA_NodeId) UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER);
            descr[0].resultMask = UA_BROWSERESULTMASK_ALL;
            descr[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
            UA_BrowseResult bResult = UA_Server_browse(server, 65535,
                    (const UA_BrowseDescription *) &descr);
            UA_BrowseDescription_deleteMembers(descr);
            free(descr);
            if (bResult.statusCode == UA_STATUSCODE_GOOD) {
                /* the Server node exists, check if the node to be added is part of its subtree */
                zkUA_checkNs0 *searchedForNode = calloc(1,
                        sizeof(zkUA_checkNs0));
                searchedForNode->searchedForNode = &node->nodeId;
                searchedForNode->result = false;
                UA_Server_forEachChildNodeCall(uaServer,
                        UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER),
                        zkUA_jsonDecode_checkChildOfNS0ServerNode,
                        (void *) searchedForNode);
                if (searchedForNode->result == true) {
                    /* If this is a server object node's (sub-)child */
                    fprintf(stderr,
                            "zkUA_addNodeInternal: Node ns=%d;i=%d is part of the Server Node's subtree\n",
                            node->nodeId.namespaceIndex,
                            node->nodeId.identifier.numeric);
                    free(searchedForNode);
                    return addNodeResult;
                }
            }
        }
        /* replicate to zk */
        sCode = zkUA_UA_Server_writeAttribute_prepareReplication(server,
                node->nodeId);
        if (sCode != UA_STATUSCODE_GOOD) { /* If replication to zk failed, delete the node from the cache */
            zkUA_UA_Server_deleteNode_dontReplicate(server, node->nodeId, 1);
        }
    }
    addNodeResult.statusCode = sCode;
    return addNodeResult;
}

/* TODO: deduplicate mzxid lookups across the different c files */
/* Intercepting function called by UA Server if it adds a node by its own volition (e.g. replication-initiated, or any other UA_Server_Add...)
 or by an over-the-network req. by a UA Client */
void zkUA_Service_AddNodes_single(UA_Server *server, UA_Session *session,
        const UA_AddNodesItem *item, UA_AddNodesResult *result,
        UA_InstantiationCallback *instantiationCallback) {
    fprintf(stderr,
            "zkUA_Service_AddNodes_single: Intercepted call to Service_AddNodes_single\n");
    uaServer = server;
    _Service_AddNodes_single(server, session, item, result,
            instantiationCallback);
    /* Get the  mzxid of the node and see if we have something new(er) */
    char *buffer = calloc(65535, sizeof(char));
    size_t buffer_len = 65535;
    struct Stat stat;
    char *nodeZkPath = zkUA_encodeZnodePath(&item->requestedNewNodeId.nodeId);
    int rc = zoo_get(zkHandle, nodeZkPath, 1 /* non-zero sets watch */, buffer,
            (int *) &buffer_len, &stat);
    free(buffer);
    if (rc != ZNONODE && rc != ZOK) /* We weren't returned stat (i.e.,rc!=ZOK) but we got an error other than no znode exists for that path */
        return;

    /* Prepare values for the hashtable */
    if (rc == ZOK) { /* If the node exists and therefore has an mzxid */
        int mzxidFresher = zkUA_checkMzxidAge(nodeZkPath,
                ((long long int *) &stat.mzxid));
        free(nodeZkPath);
        /* Let's see if the mzxid for this node path exists in our hashtable or if the retrieved data is fresher */
        if (mzxidFresher <= 0) { /* I have something as old or older than what's on zk */
            return;
        }
    }
    free(nodeZkPath);
    /* Otherwise the node doesn't exist or we have something fresher
     replicate the node to zookeeper*/
    void *data = item->nodeAttributes.content.decoded.data;
    zkUA_UA_Server_replicateNode(item->nodeClass,
            item->requestedNewNodeId.nodeId, item->parentNodeId.nodeId,
            item->referenceTypeId, data);
}
