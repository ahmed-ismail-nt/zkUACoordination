/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * cli.c is a example/sample C client shell for ZooKeeper. It contains
 * basic shell functionality which exercises some of the features of
 * the ZooKeeper C client API. It is not a full fledged client and is
 * not meant for production usage - see the Java client shell for a
 * fully featured shell.
 */

/**
 * open62541 libraries
 */
#include <signal.h>
#include <stdlib.h>
#include <open62541.h>
#include <nodeset.h>
#include <zk_cli.h>
#include <zk_serverReplicate.h>
#include <zk_global.h>
#include <zk_intercept.h>
#include <pthread.h>

/**
 * ZooKeeper libraries
 */

#include <zookeeper.h>
#include <proto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#ifdef YCA
#include <yca/yca.h>
#endif

#define _LL_CAST_ (long long)

/**
 * Declarations for open62541
 */
UA_Logger logger = UA_Log_Stdout;
UA_Boolean running = true;
UA_Server *server;
UA_ServerNetworkLayer nl;
zhandle_t *zh;
zhandle_t *zkHandle; /* Global variable */
UA_Boolean addressSpaceExists = false;

static clientid_t myid;

static int to_send = 0;
static int sent = 0;
static int recvd = 0;

static int verbose = 0;

void intHandler(int signum) {
    running = false;
}

static void zkUA_writeServerStatus(UA_Int32 serverState) {
    /* Server states:
     UA_SERVERSTATE_RUNNING = 0,
     UA_SERVERSTATE_FAILED = 1,
     UA_SERVERSTATE_NOCONFIGURATION = 2,
     UA_SERVERSTATE_SUSPENDED = 3,
     UA_SERVERSTATE_SHUTDOWN = 4,
     UA_SERVERSTATE_TEST = 5,
     UA_SERVERSTATE_COMMUNICATIONFAULT = 6,
     UA_SERVERSTATE_UNKNOWN = 7
     */
    UA_VariableAttributes variableAttributes;
    UA_VariableAttributes_init(&variableAttributes);
    UA_Int32 newVal = serverState;
    UA_Variant_setScalar(&variableAttributes.value, &newVal,
            &UA_TYPES[UA_TYPES_INT32]);
    UA_Server_writeValue(server,
            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERSTATUS_STATE),
            variableAttributes.value);
}

/* Connects/Disconnects to/from endpoint using S7 protocol */
static UA_StatusCode zkUA_activateServer(void *methodHandle,
        const UA_NodeId objectId, size_t inputSize, const UA_Variant *input,
        size_t outputSize, UA_Variant *output) {
    /* Input a binary 1/0 1:activate server 0: deactivate server */
    UA_Int32 *arg = (UA_Int32*) input[0].data;
    UA_Int32 result = -1;
    if (*arg == 0) {
        /* Disconnect from endpoint and set server status to SUSPENDED */
        zkUA_writeServerStatus(3);
        result = UA_STATUSCODE_GOOD;
    } else if (*arg == 1) {
        /* Connect to endpoint and set server status to RUNNING */
        zkUA_writeServerStatus(0);
        result = UA_STATUSCODE_GOOD;
    } else {
        result = UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    UA_Variant_setScalarCopy(output, &result, &UA_TYPES[UA_TYPES_INT32]);
    return UA_STATUSCODE_GOOD;
}

/* Adds a method node to the UA server to allow for activation/deactivation of server */
void zkUA_createActivationMethod() {

    UA_MethodAttributes addmethodattributes;
    UA_MethodAttributes_init(&addmethodattributes);
    addmethodattributes.displayName = UA_LOCALIZEDTEXT("en_US",
            "Activate/Deactivate Server");
    addmethodattributes.executable = true;
    addmethodattributes.userExecutable = true;

    UA_Argument inputArguments;
    UA_Argument_init(&inputArguments);
    inputArguments.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    inputArguments.description = UA_LOCALIZEDTEXT("en_US",
            "Activate/Deactivate Server");
    inputArguments.name = UA_STRING("Input");
    inputArguments.valueRank = -1; //uaexpert will crash if set to 0 ;)

    UA_Argument outputArguments;
    UA_Argument_init(&outputArguments);
    outputArguments.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    outputArguments.description = UA_LOCALIZEDTEXT("en_US",
            "Activation/Deactivation Result");
    outputArguments.name = UA_STRING("Output");
    outputArguments.valueRank = -1;

    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1, 30001),
            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERREDUNDANCY),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "ModifyServerStatus"), addmethodattributes,
            &zkUA_activateServer, /* callback of the method node */
            NULL, /* handle passed with the callback */
            1, &inputArguments, 1, &outputArguments, NULL);
}

/* Creates a GroupId node under the ServerRedundancy Node to hold the Redundancy Group's GUID */
void zkUA_createGroupId(UA_Guid *guid) {

    fprintf(stderr, "setting up groupGuid var: "UA_PRINTF_GUID_FORMAT"\n",
            UA_PRINTF_GUID_DATA(*guid));

    /* Create a GroupId for the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    attr.valueRank = -2;
    attr.dataType = UA_TYPES[UA_TYPES_GUID].typeId;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", "GroupGUID");
    UA_QualifiedName qualifiedName = UA_QUALIFIEDNAME_ALLOC(1, "GroupGUID");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
    attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
    UA_Variant_setScalar(&attr.value, guid, &UA_TYPES[UA_TYPES_GUID]);
    UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 30000),
            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERREDUNDANCY),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), qualifiedName,
            UA_NODEID_NULL, attr, NULL, NULL);
    UA_Server_writeBrowseName(server, UA_NODEID_NUMERIC(1, 30000),
            qualifiedName);
    UA_LocalizedText_deleteMembers(&attr.displayName);
    UA_QualifiedName_deleteMembers(&qualifiedName);
}

void zkUA_createNodeset(UA_Guid *data) {

    UA_StatusCode statuscode;
    if (UA_nodeset(server) != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(logger, UA_LOGCATEGORY_SERVER,
                "Namespace index for generated nodeset does not match. The call to the generated method has to be before any other namespace add calls.");
        statuscode = (int) UA_STATUSCODE_BADUNEXPECTEDERROR;
        fprintf(stderr, "zkUA_createNodeset: Exiting with code %d\n",
                statuscode);
        running = false;
        pthread_exit(&statuscode);
    } else
        fprintf(stderr, "zkUA_createNodeset: Created nodeset\n");
    /* read and set watches on the entire addressSpace on zk */
    zoo_aget_children(zh, zkUA_zkServAddSpacePath(), 1,
            zkUA_UA_Server_replicateZk_getNodes,
            strdup(zkUA_zkServAddSpacePath()));
    zkUA_createGroupId(data);
    zkUA_createActivationMethod();
}

static void zkUA_checkAddressSpaceExists(int rc,
        const struct String_vector *strings, const void *data) {
    UA_StatusCode statuscode;
    if (rc != ZOK) { /* it must have been initialized by UA_Server_run or by another server by now */
        fprintf(stderr, "ZooKeeper error %d\n", rc);
        pthread_exit(&statuscode);
    }
    /* If the address space was initialized on zk by another server */
    if (addressSpaceExists == true) {
        /* loop through all of the returned zk children */
        if (strings) {
            if (strings->count > 0) {
                /* Initialize replication */
                zkUA_UA_Server_replicateZk(zh, zkUA_zkServAddSpacePath(),
                        server);
                zkUA_createActivationMethod();
            }
        }
    } else { /* If we are the ones initializing the addressSpace on zk */
        /* create nodes from nodeset */
        zkUA_createNodeset((UA_Guid *) data);
    }
    return;
}

static void zkUA_addressSpaceWatcher(zhandle_t *zzh, int type, int state,
        const char *path, void* context) {

    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    /* Be careful using zh here rather than zzh - as this may be mt code
     * the client lib may call the watcher before zookeeper_init returns */
    fprintf(stderr, "Watcher %s state = %s", zkUA_type2String(type),
            zkUA_state2String(state));
    /* Disable replication to zk since we are now copying from zk */
    if (path && strlen(path) > 0) {
        fprintf(stderr, " for path %s\n", path);
        char *termPath = calloc(65535, sizeof(char));
        snprintf(termPath, 65535, "%s", path);

        if (type == ZOO_DELETED_EVENT) {
            /* A node was deleted */
            /* Extract the nsIndex and nodeId */
            char *storeNs = calloc(65535, sizeof(char));
            char *storeId = calloc(65535, sizeof(char));
            zkUA_extractNsIndexNodeId(&termPath, storeNs, storeId);
            long long ns, id;
            ns = strtoll(storeNs, NULL, 10);
            id = strtoll(storeId, NULL, 10);
            fprintf(stderr,
                    "zkUA_addressSpaceWatcher: Deleting node ns=%lu id=%lu\n",
                    ns, id);
            sCode = zkUA_UA_Server_deleteNode_dontReplicate(server,
                    UA_NODEID_NUMERIC(ns, id), true /* delete references */);
        } else if (type == ZOO_CHANGED_EVENT) {
            /* A node was modified - get the node, decode and try to add it's values and */
            fprintf(stderr,
                    "zkUA_addressSpaceWatcher: Getting and adding node %s\n",
                    termPath);
            zkUA_jsonDecode_zkNode(termPath, server); /* gets the path's data and checks the hashtable */
        } else if (type == ZOO_CHILD_EVENT) {
            /* A node was created/deleted */
            zkUA_UA_Server_replicateZk(zzh, zkUA_zkServAddSpacePath(), server);
            fprintf(stderr,
                    "zkUA_addressSpaceWatcher: A node was created or deleted under %s - replicating full addressSpace\n",
                    termPath);
        }
        free(termPath);
    }
    fprintf(stderr, "\n");
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            const clientid_t *id = zoo_client_id(zzh);
            if (myid.client_id == 0 || myid.client_id != id->client_id) {
                myid = *id;
                fprintf(stderr, "Got a new session id: 0x%llx\n",
                _LL_CAST_ myid.client_id);
            }
        } else if (state == ZOO_AUTH_FAILED_STATE) {
            fprintf(stderr, "Authentication failure. Shutting down...\n");
            zookeeper_close(zzh);
            zh = 0;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            fprintf(stderr, "Session expired. Shutting down...\n");
            zookeeper_close(zzh);
            zh = 0;
        }
    }
}

static void init_UA_Server(void *retval, zkUA_Config *zkUAConfigs) {

    UA_StatusCode *statuscode = (UA_StatusCode *) retval;

    /* Initialize address space path on zookeeper */
    char *groupGuid = calloc(65535, sizeof(char));
    snprintf(groupGuid, 65535, UA_PRINTF_GUID_FORMAT,
            UA_PRINTF_GUID_DATA(zkUAConfigs->guid));
    zkUA_initializeZkServAddSpacePath(groupGuid, zh);
    free(groupGuid);
    /* Initialize the hashmap that holds nodes' mZxid's */
    zkUA_initializeHashmap();
    zkUA_initializeAvailabilityPriority(zkUAConfigs->aPriority);
    /* Check if the address space exists */
    struct String_vector strings;
    int rc = zoo_get_children(zh, zkUA_zkServAddSpacePath(), 0, &strings);
    if (rc == ZOK) {
        if (&strings) {
            if (strings.count > 0) {
                addressSpaceExists = true;
                fprintf(stderr, "The path exists and there are strings!\n");
            }
        }
    } else { /* error of some sort - the path should exist by now */ ///stopHandler(SIGINT); /* exit program */
        free_zkUAConfigs(zkUAConfigs);
        pthread_exit(&statuscode);
    }
    /* initialize the server */
    UA_ServerConfig config = UA_ServerConfig_standard;
    nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard,
            zkUAConfigs->uaPort);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    /* creates the server, namespaces, endpoints, sets the security configs etc., using the UA_ServerConfig defined above */
    server = UA_Server_new(config);
    /* More initializations */
    zkUA_initializeUaServerGlobal((void *) server);

    switch (zkUAConfigs->rSupport) {
    case (2): /* Warm Redundancy */
    case (3): { /* Hot redundancy */
        /* Replicate or initialize addressSpace */
        zoo_aget_children(zh, zkUA_zkServAddSpacePath(), 0,
                zkUA_checkAddressSpaceExists, &zkUAConfigs->guid);
        if (!zkUAConfigs->state) { /* inactive server - await activation signal from the failover controller */
            /* write server status as suspended */
            zkUA_writeServerStatus(3);
        }
        break;
    }
    case (0): /* Standalone server */
    case (1): /* Cold redundancy */
    case (4): /* Transparent Redundancy */
    case (5): { /* Hot+ Redundancy */
        if (zkUAConfigs->state) { /* active server */
            zoo_aget_children(zh, zkUA_zkServAddSpacePath(), 0,
                    zkUA_checkAddressSpaceExists, &zkUAConfigs->guid);
            /* create thread to monitor changes to address space and apply them locally */
        } else { /* inactive server - error*/
            fprintf(stderr,
                    "init_UA_server: Error! Initialized as an inactive server. Exiting...\n");
            pthread_exit(&statuscode); /* race? */
        }
        break;
    }
    }

    /* start server */
    statuscode = UA_Server_run(server, &running); //UA_blocks until running=false
    /* ctrl-c received -> clean up */
    UA_Server_delete(server);
    nl.deleteMembers(&nl);
    zkUA_destroyHashtable();
    free_zkUAConfigs(zkUAConfigs);
    fprintf(stderr, "init_UA_Server: Exiting with code %d\n", statuscode);
}

int main() {
    /* catches ctrl-c */
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = intHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    /* Read the config file */
    zkUA_Config zkUAConfigs;
    zkUA_readConfFile("serverConf.txt", &zkUAConfigs);

    char buffer[4096];
    char p[2048];
    /* dummy certificate */
    strcpy(p, "dummy");
    verbose = 0;
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    zoo_deterministic_conn_order(1); // enable deterministic order
    zkHandle = zookeeper_init(zkUAConfigs.zooKeeperQuorum,
            zkUA_addressSpaceWatcher, 30000, &myid, 0, 0);
    /* set global zookeeper handle variable */
    zh = zkHandle;
    fprintf(stderr, "cli_UA_server: initialized zkHandle\n");
    if (!zh) {
        return errno;
    }

    UA_StatusCode *retval;
    init_UA_Server((void *) retval, &zkUAConfigs);
    if (to_send != 0)
        fprintf(stderr, "Recvd %d responses for %d requests sent\n", recvd,
                sent);
    zookeeper_close(zh);
    return 0;
}

