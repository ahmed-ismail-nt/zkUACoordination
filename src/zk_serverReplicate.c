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
#include <stdio.h>
#include <assert.h>
#include <simple_parse.h>
#include <stdlib.h>
#include <zk_serverReplicate.h>
#include <zk_intercept.h>
#include <zk_cli.h>
#include <zk_global.h>
#include "hashtable/hashtable.h"
UA_Server *server = NULL;
/* The server path on zk */
char *zkServerPath;
UA_Boolean replicateNode = true; /* Flag - replicate any node add/delete/update operations to ZooKeeper */
UA_Boolean availabilityPriority = false;
/**
 * zkUA_initializeRedundancy:
 * Initializes the ServerUriArray variale to hold the URI of all redundant servers of the OPC UA Server.
 * See Pg 13 of Part 5 of OPC UA Spec. R1.03
 *
 * todo: Add functions to extend the array with new URIs or to remove URIs.
 *
 */
void zkUA_initializeRedundancy(/* should probably take an array length as a variable here */) {

    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    attr.valueRank = -2;
    attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    attr.displayName = UA_LOCALIZEDTEXT("en_US", "ServerUriArray");
    UA_QualifiedName qualifiedName = UA_QUALIFIEDNAME(1, "ServerUriArray");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
    attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;

    /* add an array of 10 strings to hold the server URIs */
    /* TODO: This should be dynamic */
    UA_Variant_setArray(&attr.value,
            UA_Array_new(10, &UA_TYPES[UA_TYPES_STRING]), 10,
            &UA_TYPES[UA_TYPES_STRING]);
    UA_StatusCode sCode = UA_Server_addVariableNode(server,
            UA_NODEID_NUMERIC(0,
                    UA_NS0ID_SERVER_SERVERREDUNDANCY_SERVERURIARRAY),
            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERREDUNDANCY),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), qualifiedName,
            UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE), attr, NULL, NULL);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr, "couldn't add serverredundancytype\n");
    }
    UA_Variant_deleteMembers(&attr.value);
    UA_LocalizedText_deleteMembers(&attr.displayName);
    UA_QualifiedName_deleteMembers(&qualifiedName);
}

void zkUA_initializeAvailabilityPriority(UA_Boolean aPriority) {
    availabilityPriority = aPriority;
}
/**
 * zkUA_UA_Server_replicateZk:
 * Replicates the address space stored on zk locally
 */
void zkUA_UA_Server_replicateZk(zhandle_t *zh, char *zkServerPath,
        UA_Server *uaServer) {

    fprintf(stderr,
            "zkUA_UA_Server_replicateZk: Replicating zkServerPath: %s\n",
            zkServerPath);
    /* Set the global zk Server and UA Server variables */
    server = uaServer;
    /* Get all zk nodes under zkServerPath */
    zoo_aget_children(zh, zkServerPath, 1 /* set watch on full addressSpace */,
            zkUA_UA_Server_replicateZk_getNodes, strdup(zkServerPath));
}

void zkUA_extractNsIndexNodeId(const void *nId, char *storeNs, char *storeId) {

    const char *nsStr = "ns=";
    const char *iStr = ";i=";

    /* locate the namespace substring and node ID substring in both strings */
    char *cmpNs = strstr(*(char * const *) nId, nsStr);
    char *cmpId = strstr(*(char * const *) nId, iStr);

    /* allocate memory for the namespace index substring */
    size_t sizeofNs = strlen(cmpNs) - strlen(nsStr) - strlen(cmpId);
    /* copy the namspace index into a temporary string */
    strncpy(storeNs, cmpNs + strlen(nsStr), sizeofNs);

    /* allocate memory for the nodeId substring */
    size_t sizeofId = strlen(cmpId) - strlen(iStr);

    /* copy the nodeId into a temporary string */
    strncpy(storeId, cmpId + strlen(iStr), sizeofId);

}

static int nodeIdCmp(const void *nId1, const void *nId2) {
    char *storeNs1 = calloc(65535, sizeof(char));
    char *storeNs2 = calloc(65535, sizeof(char));
    char *storeId1 = calloc(65535, sizeof(char));
    char *storeId2 = calloc(65535, sizeof(char));

    zkUA_extractNsIndexNodeId(nId1, storeNs1, storeId1);
    zkUA_extractNsIndexNodeId(nId2, storeNs2, storeId2);
    /* convert the namespace Ids from string to long long int */
    long long Ns1, Ns2;
    Ns1 = strtoll(storeNs1, NULL, 10);
    Ns2 = strtoll(storeNs2, NULL, 10);

    /* compare the namespace Ids */
    if (Ns1 != Ns2) {
        free(storeNs1);
        free(storeNs2);
        free(storeId1);
        free(storeId2);
        if (Ns1 < Ns2)
            return -1;
        if (Ns1 > Ns2)
            return 1;
    }

    /* allocate memory for the nodeId substring */
    /* convert the node Ids from string to long long int */
    long long Id1, Id2;
    Id1 = strtoll(storeId1, NULL, 10);
    Id2 = strtoll(storeId2, NULL, 10);

    /* free memory */
    free(storeNs1);
    free(storeNs2);
    free(storeId1);
    free(storeId2);

    /* compare the node Ids */
    if (Id1 < Id2)
        return -1;
    if (Id1 > Id2)
        return 1;
    /* otherwise they are equal */
    return 0;

}

void zkUA_UA_Server_replicateZk_getNodes(int rc,
        const struct String_vector *strings, const void *data) {

    fprintf(stderr, "Data completion %s rc = %d\n", (char*) data, rc);

    /* loop through all of the returned zk children */
    if (strings) {
        /* Sort array of returned children so that we add the nodes in ascending order of
         node IDs (assuming of course that a child will never have a smaller NodeId than a parent) */
        qsort(strings->data, strings->count, sizeof(char *), nodeIdCmp);
        for (int i = 0; i < (strings->count); i++) {
            fprintf(stderr, "zkUA_UA_Server_replicateZk_getNodes\t%s\n",
                    strings->data[i]);
            /* should we be passing server to the function even though it's set globally?
             could stop using the global one if we decode the server address from the
             data string. */
            char *zkNodePath = (char *) calloc(65535, sizeof(char));
            snprintf(zkNodePath, 65535, "%s/%s", (char *) data,
                    strings->data[i]);
            zkUA_jsonDecode_zkNode(zkNodePath, server);
            free(zkNodePath);
        }
    }
    free((void *) data);
}

int zkUA_checkMzxidAge(char *nodeZkPath, long long *mzxid) {

    fprintf(stderr, "zkUA_jsonDecode_zkNode: nodeZkPath %s - mzxid = %lld\n",
            nodeZkPath, *mzxid);
    /* Let's see if the mzxid for this node path exists or if the retrieved data is fresher */
    long long *val = hashtable_search(nodeMzxid, (void *) nodeZkPath);
    if (val != NULL) { /* a value for this path is stored in the hashtable */
        fprintf(stderr,
                "zkUA_jsonDecode_zkNode: nodeZkPath %s - mzxid = %lld - val = %lld\n",
                nodeZkPath, *mzxid, *val);
        if (*val > *mzxid)
            return 1; /* we have something fresher than what's on zk */
        if (*val == *mzxid)
            return 0; /* we have something as fresh as what's on zk */
    }
    return -1; /* we have something in the local cache that's older than what's on zk */
}

UA_StatusCode zkUA_insertMzxidAge(char *nodeZkPath, long long *nodeMzxidLL) {

    char *hashNodeZkPath = strdup(nodeZkPath);
    long long *mzxid = calloc(1, sizeof(long long));
    *mzxid = *nodeMzxidLL;
    if (hashtable_insert(nodeMzxid, (void *) hashNodeZkPath, (void *) mzxid)
            != 0) { /* successfully inserted */
        return UA_STATUSCODE_GOOD;
    } else
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
}

/* Delete a node's key-value pair from the local hashtable */
UA_StatusCode zkUA_deleteMzxidAge(char *nodeZkPath) {

    char *hashNodeZkPath = strdup(nodeZkPath);
    if (hashtable_remove(nodeMzxid, (void *) hashNodeZkPath) != NULL)
        return UA_STATUSCODE_GOOD;
    else
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
}

UA_StatusCode zkUA_jsonDecode_zkNode(char * nodeZkPath, UA_Server *serverDecode) {
    /* get the node from zk and send it to my_silent_data_completion */
    char *buffer = calloc(65535, sizeof(char));
    size_t buffer_len = 65535;
    struct Stat stat;
    int rc = zoo_get(zkHandle, nodeZkPath, 1 /* non-zero sets watch */, buffer,
            (int *) &buffer_len, &stat);
    if (rc) {
        fprintf(stderr, "\t Error %d for %s\n", rc, nodeZkPath);
        free(buffer);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    /* Prepare values for the hashtable */
    int mzxidFresher = zkUA_checkMzxidAge(nodeZkPath,
            ((long long int *) &stat.mzxid));
    if (mzxidFresher >= 0) { /* we have something fresher or as fresh as what's on zk */
        free(buffer);
        return UA_STATUSCODE_GOOD;
    }
    /* otherwise what we just got is fresher */
    /* Let's add the path and mzxid to the hashtable */
    zkUA_insertMzxidAge(nodeZkPath, ((long long int *) &stat.mzxid));
    /* decode and add the node */
    zkUA_jsonDecode_zkNodeToUa(rc, buffer, buffer_len, &stat, server);
    free(buffer);
    return UA_STATUSCODE_GOOD;
}

/*
 * Search through the entire namespace starting from the NS0 Server Node looking for a specific child.
 * Return true if it is found as a sub-child of NS0
 */
int cntrNS0 = 0, nServerNS0 = 3000, visitedNodeIDServerNS0[3000];

UA_StatusCode zkUA_jsonDecode_checkChildOfNS0ServerNode(UA_NodeId childId,
        UA_Boolean isInverse, UA_NodeId referenceTypeId,
        void *searchedForNodeId) {

    if (isInverse)
        return UA_STATUSCODE_GOOD;
    for (size_t j = 0; j < nServerNS0; j++) {
        if (visitedNodeIDServerNS0[j] == childId.identifier.numeric) {
            return UA_STATUSCODE_GOOD;
        }
    }

    UA_NodeId *searchedForNode =
            ((zkUA_checkNs0 *) searchedForNodeId)->searchedForNode;
    bool *result = &((zkUA_checkNs0 *) searchedForNodeId)->result;
    if (childId.namespaceIndex == searchedForNode->namespaceIndex
            && childId.identifier.numeric
                    == searchedForNode->identifier.numeric) {
        *result = true;
    }
    visitedNodeIDServerNS0[cntrNS0++] = childId.identifier.numeric;
    UA_Server_forEachChildNodeCall(server, childId,
            zkUA_jsonDecode_checkChildOfNS0ServerNode,
            (void *) searchedForNodeId);
    return UA_STATUSCODE_GOOD;
}

