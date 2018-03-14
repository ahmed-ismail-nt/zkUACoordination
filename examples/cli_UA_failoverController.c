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
#include <zk_global.h>
#include <pthread.h>
#include <zk_clientReplicate.h>
#include <zk_urlEncode.h>

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

UA_Logger logger = UA_Log_Stdout;
UA_Boolean running = true;
static zhandle_t *zh;
static clientid_t myid;
struct timeval startTime;

static int sent = 0;
static int recvd = 0;

volatile sig_atomic_t stopMonitoring;

static char *zkRedundancyActiveNode;
static char *zkRedundancyActivePath;
static char *zkRedundancyPath;
static char *serverUri;
static char *username, *password;
static char *encodedServerUri;
UA_Client *client;
int failureCounter = 0;
int rSupport = -1;

static int verbose = 0;

void intHandler(int signum) {
    stopMonitoring = 1;
    UA_Client_delete(client);
    client = NULL;
}

/* Reads node's UA_Int32 value */
static UA_Int32 zkUA_readValue(UA_Client *client, UA_Int32 nodeIdNumeric) {
    int retval;
    UA_Int32 value = -1;
    printf("\nReading the value of the node (0, %d):\n", nodeIdNumeric);
    if (client == NULL)
        return -1;
    UA_Variant *val = UA_Variant_new();
    retval = UA_Client_readValueAttribute(client,
            UA_NODEID_NUMERIC(0, nodeIdNumeric), val);
    if (retval == UA_STATUSCODE_GOOD) {
        failureCounter = 0;
        value = *(UA_Int32*) val->data;
        printf("the value is: %i\n", value);
    } else {
        failureCounter++;
        fprintf(stderr,
                "zkUA_readValue: Could not read the value of node (0, %d)\n",
                nodeIdNumeric);
        if (failureCounter == 3) {
            intHandler(SIGINT);
        }
    }
    UA_Variant_delete(val);
    return value;
}

/* Read node's redundancySupport level */
static int zkUA_readRedundancySupport(UA_Client *client) {
    return zkUA_readValue(client,
            UA_NS0ID_SERVER_SERVERREDUNDANCY_REDUNDANCYSUPPORT);
}

/* Read node's state and serviceLevel */
static UA_StatusCode zkUA_readState(UA_Client *client) {

    /* Get the UA Server State - Pg 84 of 123 of UA Spec. R1.03*/
    /* RUNNING_0 FAILED_1 NO_CONFIGURE_2 SUSPENDED_3 SHUTDOWN_4 TEST_5 COMMUNICATION_FAULT_6 UNKNOWN_7 */
    UA_Int32 value = -1;
    value = zkUA_readValue(client, UA_NS0ID_SERVER_SERVERSTATUS_STATE);
    if (value == 0) {
        fprintf(stderr, "cli_UA_failoverController: server is running\n");
        return UA_STATUSCODE_GOOD;
    } else
        return UA_STATUSCODE_BADUNEXPECTEDERROR; /* TODO: Correct assignment of errors */
}

static UA_StatusCode zkUA_readServiceLevel(UA_Client *client) {
    /* Get the UA Server ServiceLevel */
    UA_Int32 value = -1;
    value = zkUA_readValue(client, UA_NS0ID_SERVER_SERVICELEVEL);
    if (value > 0) {
        fprintf(stderr,
                "cli_UA_failoverController: the value of the ServiceLevel node (0, 2267) is: %d\n",
                value);
    }
    return UA_STATUSCODE_GOOD;
}

static void zkUA_monitorServer(UA_Client *client) {
    UA_StatusCode sCode;
    while (!stopMonitoring) {
        sCode = zkUA_readState(client);
        /* If server timesout or server state is bad then quit */
        if (sCode != UA_STATUSCODE_GOOD) {
            return;
        }
        /* Sleep for a certain amount of time before repeating */
        sleep(3);
    }

}

static UA_StatusCode zkUA_clientConnectToServer(void *retval) {

    UA_StatusCode *statuscode = retval;
    client = UA_Client_new(UA_ClientConfig_standard);

    /* Listing endpoints */
    UA_EndpointDescription* endpointArray = NULL;
    size_t endpointArraySize = 0;

    statuscode = UA_Client_getEndpoints(client, serverUri, &endpointArraySize,
            &endpointArray);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_Array_delete(endpointArray, endpointArraySize,
                &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
        UA_Client_delete(client);
        client = NULL;
        return retval;
    }
    printf("%i endpoints found\n", (int) endpointArraySize);
    for (size_t i = 0; i < endpointArraySize; i++) {
        printf("URL of endpoint %i is %.*s\n", (int) i,
                (int) endpointArray[i].endpointUrl.length,
                endpointArray[i].endpointUrl.data);
    }
    UA_Array_delete(endpointArray, endpointArraySize,
            &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);

    /* Connect to a server */
//    retval = UA_Client_connect_username(client, serverUri, username, password); //Connect with user/pass
    retval = UA_Client_connect(client, serverUri); //anonymous connect
    if (retval != UA_STATUSCODE_GOOD) {
        fprintf(stderr, "zkUA_clientConnectToServer: Couldn't connect to %s\n",
                serverUri);
        UA_Client_delete(client);
        client = NULL;
        return retval;
    }

    return retval;
}

static int zkUA_getActiveNodeLock() {
    int rc = -1;
    int flags = 0;
    char *path_buffer = calloc(65535, sizeof(char));
    int path_buffer_len = strlen(path_buffer);
    /* Try to get a lock on the activeNode path */
    fprintf(stderr, "zkUA_getActiveNodeLock: Trying to get lock on %s\n",
            zkRedundancyActiveNode);
    flags |= ZOO_EPHEMERAL;
    rc = zoo_create(zh, zkRedundancyActiveNode, " ", strlen(" "),
            &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
    free(path_buffer);
    return rc;
}

int remainInactive = 1;
static void zkUA_zkEvaluateActiveNodes(const struct String_vector *strings) {
    /* Currently limited to checking if the system should create an active znode lock
     * and activate the server.
     */

    UA_StatusCode *sCode;
    UA_StatusCode statuscode = UA_STATUSCODE_BADUNEXPECTEDERROR;
    int rc = -1;
    if (strings) {
        switch (rSupport) {
        case 0: /* standalone */
        case 1: /* cold */
        case 2: { /* warm */
            if (strings->count > 0) {
                fprintf(stderr,
                        "zkUA_zkEvaluateActiveNodes: There are active nodes. Only one active node is permitted at a time.\n");
                for (int i = 0; i < (strings->count); i++) {
                    fprintf(stderr, "zkUA_zkEvaluateActiveNodes: The active node is %s\n",
                            strings->data[i]);
                }
                break; /* exit switch */
            }
            /* Don't Break, continue on to get the lock as would be done in case 3-5 */
        }
        case 3: /* hot - currently treated as all active and connected to a downstream device */
        case 4: /* transparent */
        case 5: { /* hot+ */
            if (strings->count > 0) {
                fprintf(stderr,
                        "zkUA_zkEvaluateActiveNodes: There are active nodes..\n");
                for (int i = 0; i < (strings->count); i++) {
                    if(strcmp(strings->data[i], encodedServerUri)==0) {
                        fprintf(stderr, "zkUA_zkEvaluateActiveNodes: I already have a lock as an active node as %s\n",
                                strings->data[i]);
                        return; /* exit switch */
                    }
                }
            } /* Otherwise, we don't have a lock and we can try to get one
            /* i.e. you can have multiple active nodes or it was case 0-2 and no other nodes are active */
            rc = zkUA_getActiveNodeLock(); /* get a lock */
            if (rc != 0) /* failed to get a lock */{
                fprintf(stderr,
                        "zkUA_zkEvaluateActiveNodess: Could not get lock on %s\n",
                        zkRedundancyActiveNode);
                return;
            }
            /* If you could get a lock and this is a cold redundancy server execute the UA Server application */
            if (rSupport == 1) {
                ssize_t chars = -1;
                char *cwd = zkUA_cleanStdoutFromCommand("pwd", &chars);
                char *execCold = calloc(65535, sizeof(char));
                snprintf(execCold, 65535, "%s/cli_mt_UA_server", cwd);
                fprintf(stderr, "zkUA_zkEvaluateActiveNodess: Activating %s\n",
                        execCold);
                /* todo: popen with a conf file that forces server into active state */
                if (popen(execCold, "r") == NULL) {
                    fprintf(stderr,
                            "zkUA_zkEvaluateActiveNodess: Failed to activate the cold server\n");
                }
                free(execCold);
            }
            /* Initialize a client to connect to the UA Server */
            int attempts = 0;
            while (statuscode != UA_STATUSCODE_GOOD && attempts < 10) {
                attempts++;
                statuscode = zkUA_clientConnectToServer(sCode);
                fprintf(stderr, "zkUA_zkEvaluateActiveNodes: statusCode = %d\n",
                        statuscode);
                sleep(3);
            }
            if (statuscode != UA_STATUSCODE_GOOD) {
                fprintf(stderr,
                        "zkUA_zkEvaluateActiveNodes: could not connect to server - bad statuscode = %d\n",
                        statuscode);
                exit(-1);
            }
            /* Watch server status and service level */
            zkUA_monitorServer(client);
            break;
        }
        default:
            break;
        } /* switch */
    } else {
        fprintf(stderr, "zkUA_zkEvaluateActiveNodess: Received no strings\n");
    }
}

void zkUA_activeNodesWatcher(zhandle_t *zzh, int type, int state,
        const char *path, void* context) {
    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    /* Be careful using zh here rather than zzh - as this may be mt code
     * the client lib may call the watcher before zookeeper_init returns */
    fprintf(stderr, "Watcher %s state = %s", zkUA_type2String(type),
            zkUA_state2String(state));
    if (path && strlen(path) > 0) {
        fprintf(stderr, " for path %s\n", path);
        /* Get the active nodes list and evaluate to see if we should activate */
        struct String_vector strings;
        zoo_get_children(zh, zkRedundancyActivePath,
                1 /*set watch to call zkUA_activeNodesWatcher on change*/,
                &strings);
        zkUA_zkEvaluateActiveNodes(&strings);
    }
}

/* init_UA_client:  */
static void init_UA_client(void* retval, zkUA_Config *zkUAConfigs) {

    UA_StatusCode *statuscode = (UA_StatusCode *) retval;

    /* Initialize address space path on zookeeper */
    char *groupGuid = calloc(65535, sizeof(char));
    snprintf(groupGuid, 65535, UA_PRINTF_GUID_FORMAT,
            UA_PRINTF_GUID_DATA(zkUAConfigs->guid));

    /* Initialize a path for redundancy servers leader election and node discovery */
    /* All of the nodes in a redundancy group need a path to sync under */
    /* Every group will have a unique groupId (UUID) under which it syncs */
    (groupGuid, zh);
    zkRedundancyPath = calloc(65535, sizeof(char));
    snprintf(zkRedundancyPath, 65535, "/Servers/%s/Redundancy", groupGuid);
    free(groupGuid);

    int flags = 0;
    char *path_buffer = calloc(65535, sizeof(char));
    int path_buffer_len = strlen(path_buffer);
    int rc = zoo_create(zh, zkRedundancyPath, " ", strlen(" "),
            &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
    if (rc) {
        fprintf(stderr, "Error %d for %s\n", rc, zkRedundancyPath);
    }
    /* String holding path for active nodes */
    zkRedundancyActivePath = calloc(65535, sizeof(char));
    snprintf(zkRedundancyActivePath, 65535, "%s/Active", zkRedundancyPath);
    /* String holding path for this node if it is to become an active node */
    serverUri = calloc(65535, sizeof(char));
    snprintf(serverUri, 65535, "opc.tcp://%s:%lu", zkUAConfigs->hostname,
            zkUAConfigs->uaPort); /* using config file hostname & port */
    encodedServerUri = zkUA_url_encode(serverUri);
    zkRedundancyActiveNode = calloc(65535, sizeof(char));
    snprintf(zkRedundancyActiveNode, 65535, "%s/%s", zkRedundancyActivePath,
            encodedServerUri);
    /* If the active node's PATH (not the actual node) doesn't exist - initialize it */
    struct Stat stat;
    rc = zoo_exists(zh, zkRedundancyActivePath, 1, &stat);
    if (rc == ZNONODE) {
        rc = zoo_create(zh, zkRedundancyActivePath, " ", strlen(" "),
                &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
        if (rc) {
            fprintf(stderr, "Error %d for zoo_acreate: %s\n", rc,
                    zkRedundancyActivePath);
        }
    } else
        fprintf(stderr, "Error %d for zoo_exists: %s\n", rc,
                zkRedundancyActivePath);

    rSupport = zkUAConfigs->rSupport;
    /* If the Server is running and is in a good serviceLevel
     add the node to the zk's list of Active/etc. servers */
    switch (rSupport) {
    case (0): { /* No redundancy */
        fprintf(stderr, "Running in standalone mode\n");
        /* Standalone server: Create lock at /Servers/GroupUUID/Redundancy/Active path */
        flags |= ZOO_EPHEMERAL;
        rc = zoo_create(zh, zkRedundancyActiveNode, " ", strlen(" "),
                &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
        if (rc != 0) //Could not create an ephemeral activeNode znode
            fprintf(stderr,
                    "Could not create an ephemeral activeNode znode at %s\n",
                    zkRedundancyActiveNode);
        /* Initialize a client to connect to the UA Server */
        zkUA_clientConnectToServer(statuscode);
        /* Watch server status and service level */
        zkUA_monitorServer(client);
        break;
    }
    default: {
        char *zkRedundancyrTypePath = calloc(65535, sizeof(char));
        if (rSupport == 1)
            snprintf(zkRedundancyrTypePath, 65535, "%s/Cold", zkRedundancyPath);
        if (rSupport == 2)
            snprintf(zkRedundancyrTypePath, 65535, "%s/Warm", zkRedundancyPath);
        if (rSupport == 3)
            snprintf(zkRedundancyrTypePath, 65535, "%s/Hot", zkRedundancyPath);
        if (rSupport == 4)
            snprintf(zkRedundancyrTypePath, 65535, "%s/Transparent",
                    zkRedundancyPath);
        if (rSupport == 5)
            snprintf(zkRedundancyrTypePath, 65535, "%s/HotMirrored",
                    zkRedundancyPath);
        flags = 0;
        /* create an ephemeral node under the redundancyType path */
        fprintf(stderr, "Creating the zkRedundancyrTypePath %s\n",
                zkRedundancyrTypePath);
        rc = zoo_create(zh, zkRedundancyrTypePath, " ", strlen(" "),
                &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
        char *zkRedundancyrTypeNode = calloc(65535, sizeof(char));
        snprintf(zkRedundancyrTypeNode, 65535, "%s/%s", zkRedundancyrTypePath,
                encodedServerUri /*serverUri*/);
        flags |= ZOO_EPHEMERAL;
        rc = zoo_create(zh, zkRedundancyrTypeNode, " ", strlen(" "),
                &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
        if (rc != 0)
            fprintf(stderr,
                    "Could not create an ephemeral activeNode znode at %s\n",
                    zkRedundancyrTypeNode);
        /* Check if there are any Active nodes and attempt to create a lock if not */
        struct String_vector strings;
        zoo_get_children(zh, zkRedundancyActivePath,
                1 /*set watch to call zkUA_activeNodesWatcher on change*/,
                &strings);
        zkUA_zkEvaluateActiveNodes(&strings);
        free(zkRedundancyrTypeNode);
        free(zkRedundancyrTypePath);
        break;
    }
    }
    while (!stopMonitoring){
        /* Do nothing:
         * - If I don't have a lock yet, wait till the watcher is triggered and we can get one
         * - If I have a lock, we're already just doing nothing but monitoring the server */
        sleep(1);
    }
    free(path_buffer);
    free_zkUAConfigs(zkUAConfigs);
    free(encodedServerUri);
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
    username = zkUAConfigs.username;
    password = zkUAConfigs.password;

    char buffer[4096];
    char p[2048];
    /* dummy certificate */
    strcpy(p, "dummy");
    verbose = 0;
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    zoo_deterministic_conn_order(1); // enable deterministic order
    zh = zookeeper_init(zkUAConfigs.zooKeeperQuorum, zkUA_activeNodesWatcher,
            30000, &myid, 0, 0);
    if (!zh) {
        return errno;
    }

    UA_StatusCode *retval;
    init_UA_client((void *) retval, &zkUAConfigs);
    zookeeper_close(zh);
    free(zkRedundancyPath);
    free(zkRedundancyActivePath);
    free(zkRedundancyActiveNode);
    free(serverUri);
    return 0;
}
