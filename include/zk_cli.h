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
#include <zookeeper.h>
#include "src/hashtable/hashtable.h"
#include <open62541.h>

typedef struct zkUA_Config {
    long int uaPort;
    int rSupport;
    int state;
    UA_Boolean aPriority;
    char *hostname;
    char *username;
    char *password;
    char *serverId;
    char *zooKeeperQuorum;
    UA_Guid guid;
} zkUA_Config;

/**
 * zkUA_initializeUaServerGlobal:
 * Initializes a global variable with the given UA Server handle.
 */
void zkUA_initializeUaServerGlobal(void *server);

/** MZXID HASHTABLE FUNCTIONS: **/

/**
 * zkUA_destroyHashtable:
 * Deletes the mzxid hashtable while free'ing all remaining values in the hashtable.
 */
void zkUA_destroyHashtable();
/**
 * zkUA_returnHashtable:
 * A getter function for the mzxid hashtable.
 */
void *zkUA_returnHashtable();
/**
 * zkUA_hash:
 * This function is sourced from: http://www.cse.yorku.ca/~oz/hash.html
 * Creates a hash from a string for use as the key in the mzxid hashtable.
 * The string supplied for the hashtable is the OPC UA node's path on zookeeper.
 */
unsigned int zkUA_hash(void *str);
/**
 * zkUA_equalKeys
 * Function for determining if two keys are equal.
 * For use with the mzxid hashtable.
 */
int zkUA_equalKeys(void *k1, void *k2);
/**
 * zkUA_initializeHashmap:
 * Creates the mzxid hashtable with a minimum initial size of 16.
 * The hashing function is zkUA_hash.
 * The function used to determine key equality is zkUA_equalKeys.
 */
void zkUA_initializeHashmap();

/** FUNCTIONS FOR ZK SERVER PATH MANIPULATION **/
/**
 * zkUA_zkServAddSpacePath:
 * Getter function for the OPC UA Server's address space path on ZooKeeper.
 */
char *zkUA_zkServAddSpacePath();
/**
 * zkUA_encodeZnodePath:
 * Creates a string with the path for an OPC UA node on ZooKeeper.
 * TODO: Support different identifier types. Currently only supports numeric identifiers.
 */
char *zkUA_encodeZnodePath(const UA_NodeId *nodeId);
/**
 * zkUA_initializeZkServAddSpacePath:
 * Initializes the zkServerAddressSpacePath string with the path to the OPC UA Server's
 * address space on ZooKeeper and creates the path on zookeeper.
 */
void zkUA_initializeZkServAddSpacePath(char *groupGuid, zhandle_t *zh);

/** FUNCTIONS FOR MANIPULATING CONFIG FILES **/
/*
 * free_zkUAConfigs:
 * Free's the struct members storing the configs extracted from the
 * config file.
 */
void free_zkUAConfigs(zkUA_Config *zkUAConfigs);
/**
 * zkUA_readConfFile:
 * Reads the supplied file and extracts the configs to the zkUAConfigs
 * struct.
 */
void zkUA_readConfFile(char *confFileName, zkUA_Config *zkUAConfigs);
/**
 * zkUA_cleanStdoutFromCommand:
 * Source: http://www.linuxquestions.org/questions/programming-9/c-c-popen-launch-process-in-specific-directory-620305/
 * executes a shell command and returns a null-terminated string of the stdout
 */
char *zkUA_cleanStdoutFromCommand(char *cmd, ssize_t *chars);

/** ZOOKEEPER CODE DECODING & PRINTING FUNCTIONS **/
/**
 * zkUA_error2String
 * Function to convert a zookeeper status code returned by a client library function call
 * from a number to a printed (fprintf) string.
 */
void zkUA_error2String(int rc);
/**
 * zkUA_state2String:
 * Function to decode a zookeeper connection state from a code to a printed string.
 * Function originally from the cli.c file from zookeeper-3.4.9 source code.
 */
const char* zkUA_state2String(int state);
/**
 * zkUA_type2String:
 * Function to decode a watcher event type from a code to a printed string.
 * Function originally from the cli.c file from zookeeper-3.4.9 source code.
 */
const char* zkUA_type2String(int state);

/** CALLBACK FUNCTIONS (mostly) FROM cli.c OF zookeeper-3.4.9 SOURCE CODE **/
/**
 * zkUA_watcher:
 * A watcher callback function used for initialization in the zkUA Client (in cli_UA_client.c).
 * Not used for anything as the client does not set watches.
 */
void zkUA_watcher(zhandle_t *zzh, int type, int state, const char *path,
        void* context);
void zkUA_dumpStat(const struct Stat *stat);
void zkUA_my_string_completion(int rc, const char *name, const void *data);
void zkUA_my_string_completion_free_data(int rc, const char *name,
        const void *data);
void zkUA_my_data_completion(int rc, const char *value, int value_len,
        const struct Stat *stat, const void *data);
void zkUA_my_silent_data_completion(int rc, const char *value, int value_len,
        const struct Stat *stat, const void *data);
void zkUA_my_strings_completion(int rc, const struct String_vector *strings,
        const void *data);
void zkUA_my_strings_stat_completion(int rc,
        const struct String_vector *strings, const struct Stat *stat,
        const void *data);
void zkUA_my_void_completion(int rc, const void *data);
void zkUA_my_stat_completion(int rc, const struct Stat *stat, const void *data);
void zkUA_my_silent_stat_completion(int rc, const struct Stat *stat,
        const void *data);
/**
 * zkUA_startsWith:
 * Compares the beginning of a string with a supplied prefix.
 * Was previously used to determine the user supplied STDIN commands for the zk cli.
 * Is currently used to determine the config file parameters.
 */
int zkUA_startsWith(const char *line, const char *prefix);

