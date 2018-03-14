AUTOMAKE_OPTIONS = foreign
INCLUDES = -I/usr/include/zookeeper -I/usr/include/ -L/usr/lib/x86_64-linux-gnu
AM_CPPFLAGS = -I${srcdir}/include
AM_CFLAGS = -Wall -Werror -std=gnu99 $(INCLUDES)
AM_CXXFLAGS = -Wall $(USEIPV6)
LIB_LDFLAGS = -no-undefined

ZKUA_SRC = /usr/include/jansson.h include/open62541.h src/open62541.c \
    include/zk_urlEncode.h src/zk_urlEncode.c \
    include/zk_clientReplicate.h src/zk_clientReplicate.c include/zk_jsonEncode.h src/zk_jsonEncode.c \
    include/zk_jsonDecode.h src/zk_jsonDecode.c include/simple_parse.h src/simple_parse.c \
    include/zk_cli.h src/zk_cli.c include/zk_serverReplicate.h src/zk_serverReplicate.c \
    include/zk_global.h \
    src/zk_intercept.c include/zk_intercept.h include/nodeset.h src/nodeset.c

HASHTABLE_SRC = src/hashtable/hashtable_itr.h src/hashtable/hashtable_itr.c \
    src/hashtable/hashtable_private.h src/hashtable/hashtable.h src/hashtable/hashtable.c

EXPORT_SYMBOLS = '(zoo_|zookeeper_|zhandle|Z|format_log_message|log_message|logLevel|deallocate_|allocate_|zerror|is_unrecoverable|UA_|zkUA_|json_|Service_|hashtable_|create_)'

lib_LTLIBRARIES = libzkua.la
libzkua_la_SOURCES = $(ZKUA_SRC) $(HASHTABLE_SRC)
libzkua_la_LIBADD = -ljansson -lzookeeper_mt
libzkua_la_LDFLAGS = $(LIB_LDFLAGS) -export-symbols-regex $(EXPORT_SYMBOLS) $(INCLUDES)

bin_PROGRAMS = cli_mt_UA_client cli_mt_UA_server cli_mt_UA_failoverController
cli_mt_UA_client_SOURCES = examples/cli_UA_client.c $(ZKUA_SRC)
cli_mt_UA_client_LDADD = libzkua.la -lpthread -ljansson -lzookeeper_mt
cli_mt_UA_client_CFLAGS = -DTHREADED -DINTERCEPT $(INCLUDES)

cli_mt_UA_server_SOURCES =  examples/cli_UA_server.c $(ZKUA_SRC)
cli_mt_UA_server_LDADD = libzkua.la -lpthread -ljansson -lzookeeper_mt
cli_mt_UA_server_CFLAGS = -DTHREADED -DINTERCEPT $(INCLUDES)

cli_mt_UA_failoverController_SOURCES =  examples/cli_UA_failoverController.c $(ZKUA_SRC)
cli_mt_UA_failoverController_LDADD = libzkua.la -lpthread -ljansson -lzookeeper_mt
cli_mt_UA_failoverController_CFLAGS = -DTHREADED -DINTERCEPT $(INCLUDES)
