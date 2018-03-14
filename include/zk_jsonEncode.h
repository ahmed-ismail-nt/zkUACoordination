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

void zkUA_jsonEncode_UA_DataTypeMember(UA_DataType *type, json_t *jsonObject);

void zkUA_jsonEncode_UA_DataType(UA_DataType *type, json_t *jsonObject);

void zkUA_jsonEncode_UA_Variant_setObjectDataAndType(json_t *jsonObject,
        json_t *data, int dataType, char *dataIndex);
void zkUA_jsonEncode_UA_Variant_setValue(const UA_DataType *type,
        void *variantValue, json_t *jsonObject, char *dataIndex);
void zkUA_jsonEncode_UA_Variant(UA_Variant *variant, json_t *jsonObject);

void zkUA_jsonEncode_UA_ExtensionObject(UA_ExtensionObject *eObject,
        json_t *jsonObject);

void zkUA_jsonEncode_UA_DataValue(UA_DataValue *dataValue, json_t *jsonObject);

void zkUA_jsonEncode_UA_DiagnosticInfo(UA_DiagnosticInfo *dInfo,
        json_t *jsonObject);

void zkUA_jsonEncode_UA_NumericRange(UA_NumericRange *nRange,
        json_t *jsonObject);

void zkUA_jsonEncode_UA_StatusCodeDescription(
        UA_StatusCodeDescription *sCDescrip, json_t *jsonObject);

void zkUA_jsonEncode_UA_Guid(UA_Guid *guid, json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_DateTimeStruct:
 * Encodes UA_DateTimeStruct into a given JSON object
 */
void zkUA_jsonEncode_UA_DateTimeStruct(UA_DateTimeStruct *dtStruct,
        json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_String:
 * Encodes a UA_String into a JSON object and returns a pointer to the object.
 */
json_t *zkUA_jsonEncode_UA_String(UA_String *uaString);

/**
 * zkUA_jsonEncode_UA_LocalizedText:
 * Encodes a UA_LocalizedText into the provided JSON object
 */
void zkUA_jsonEncode_UA_LocalizedText(UA_LocalizedText *lText,
        json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_NodeId:
 * Encodes a UA_NodeID into the provided JSON object
 */
UA_StatusCode zkUA_jsonEncode_UA_NodeId(UA_NodeId *nodeId, json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_ExpandedNodeId:
 * Encodes a UA_ExpandedNodeId into the provided JSON object
 */
void zkUA_jsonEncode_UA_ExpandedNodeId(UA_ExpandedNodeId *expId,
        json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_QualifiedName:
 * Encodes a UA_QualifiedName into the provided JSON object
 */
void zkUA_jsonEncode_UA_QualifiedName(UA_QualifiedName *qualName,
        json_t *jsonObject);

/**
 * zkUA_jsonEncode_UA_ReferenceDescription
 * Groups together all of the various components of
 * a UA_ReferenceDescription that are already encoded
 * as JSON objects under a single provided JSON object
 * representing the ReferenceDescription.
 */
void zkUA_jsonEncode_UA_ReferenceDescription(json_t * referenceTypeId,
        json_t * isForward, json_t *nodeId, json_t *browseName,
        json_t *displayName, json_t *nodeClass, json_t *typeDefinition,
        json_t *jsonObject);

/**
 * zkUA_jsonEncodeReferences:
 * Given a browse response containing all of the
 * references of a node, this function creates a JSON
 * object containing all said references.
 */
void zkUA_jsonEncodeReferences(UA_BrowseResponse *childRef, json_t *jsonRef);

/**
 * zkUA_jsonEncodeAttributes:
 * Given all of the attributes of a node, regardless of its
 * nodeclass, this function encodes all said attributes
 * into the provided JSON object.
 */
void zkUA_jsonEncodeAttributes(UA_ReferenceDescription *childRef,
        void *nodeAttributes, json_t *jsonAtt);

