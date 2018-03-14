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
#include <jansson.h>
#include <open62541.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zookeeper.h>
/**
 * zkUA_jsonDecode_zkNodeToUa:
 * Decodes the data of a single znode transforming it into OPC UA.
 * todo: seperate the class-based JSON decoding from the UA Server node creating code to allow for 
 *       the reuse of the json decoding code.
 */
void zkUA_jsonDecode_zkNodeToUa(int rc, const char *value, int value_len,
        const struct Stat *stat, const void *data);

/**
 * zkUA_jsonDecode_zkNodeToUa_commonAttributes:
 * Decodes the following common attributes from a JSON object and places it in a given attributes struct:
 * DisplayName, Description, WriteMask, UserWriteMask
 * They are common to all node classes
 */
UA_StatusCode zkUA_jsonDecode_zkNodeToUa_commonAttributes(json_t *attributes,
        void *opcuaAttributes);

/**
 * zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType
 * Decodes the attributes that are common to Variable and VariableType node classes
 */
UA_StatusCode zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType(
        json_t *attributes, void *opcuaAttributes);

/**
 * zkUA_jsonDecode_X:
 * Decodes a JSON object containing an OPC UA node structure.
 */
UA_StatusCode zkUA_jsonDecode_UA_NodeId(json_t *nodeId, UA_NodeId *uaNodeId);

UA_StatusCode zkUA_jsonDecode_UA_LocalizedText(json_t *localizedText,
        UA_LocalizedText *destination);
char *zkUA_jsonDecode_UA_String(json_t *string);
UA_StatusCode zkUA_jsonDecode_UA_Guid(json_t *guid, UA_Guid *g);

UA_StatusCode zkUA_jsonDecode_UA_ExpandedNodeId(json_t *jsonObject,
        UA_ExpandedNodeId *eNodeId);
UA_StatusCode zkUA_jsonDecode_UA_QualifiedName(json_t *jsonObject,
        UA_QualifiedName *qName);
UA_StatusCode zkUA_jsonDecode_UA_ExtensionObject_Decoded(json_t *jsonObject,
        UA_ExtensionObject *eObject);
UA_StatusCode zkUA_jsonDecode_UA_ExtensionObject(json_t *jsonObject,
        UA_ExtensionObject *eObject);
UA_StatusCode zkUA_jsonDecode_UA_DataValue(json_t *jsonObject,
        UA_DataValue *dValue);
UA_StatusCode zkUA_jsonDecode_UA_Variant_setData(void *data,
        const UA_DataType *type, json_t *dataValue);
UA_StatusCode zkUA_jsonDecode_UA_Variant(json_t *value, UA_Variant *variant);
UA_StatusCode zkUA_jsonDecode_writeVariableAttributes(UA_Server *server,
        UA_NodeId *uaNodeId, UA_VariableAttributes *vAtt);
