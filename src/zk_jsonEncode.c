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
#include <zk_jsonEncode.h>
#include <zk_intercept.h>
/* for debugging */
#include <simple_parse.h>

/** JSON Encoding Functions **/

void zkUA_jsonEncode_UA_DataTypeMember(UA_DataType *type, json_t *jsonObject) {
    UA_UInt16 membersSize = type->membersSize;
    for (int i = 0; i < membersSize; i++) {
        json_t *member = json_object();
#ifdef UA_ENABLE_TYPENAMES
        json_t *memberName = json_string(type->members[i].memberName);
        json_object_set_new(member, "typeName", memberName);
#endif
        json_t *memberTypeIndex = json_integer(
                type->members[i].memberTypeIndex);
        json_t *padding = json_integer(type->members[i].padding);
        json_t *namespaceZero = json_boolean(type->members[i].namespaceZero);
        json_t *isArray = json_boolean(type->members[i].isArray);

        json_object_set_new(member, "memberTypeIndex", memberTypeIndex);
        json_object_set_new(member, "padding", padding);
        json_object_set_new(member, "namespaceZero", namespaceZero);
        json_object_set_new(member, "isArray", isArray);
        char *memberIndex = (char *) calloc(65535, sizeof(char));
        snprintf(memberIndex, 65535, "member[%i]", i);
        json_object_set_new(jsonObject, memberIndex, member);
        free(memberIndex);
    }
}

void zkUA_jsonEncode_UA_DataType(UA_DataType *type, json_t *jsonObject) {

#ifdef UA_ENABLE_TYPENAMES
    json_t *typeName = json_string(type->typeName);
    json_object_set_new(jsonObject, "typeName", typeName);
#endif
    json_t *typeId = json_object();
    zkUA_jsonEncode_UA_NodeId(&type->typeId, typeId);

    json_t *memSize = json_integer(type->memSize);
    json_t *typeIndex = json_integer(type->typeIndex);
    json_t *membersSize = json_integer(type->membersSize);
    json_t *builtin = json_boolean(type->builtin);
    json_t *fixedSize = json_boolean(type->fixedSize);
    json_t *overlayable = json_boolean(type->overlayable);
    json_t *binaryEncodingId = json_integer(type->binaryEncodingId);

    json_t *members = json_object();
    zkUA_jsonEncode_UA_DataTypeMember(type, members);

    json_object_set_new(jsonObject, "typeId", typeId);
    json_object_set_new(jsonObject, "memSize", memSize);
    json_object_set_new(jsonObject, "typeIndex", typeIndex);
    json_object_set_new(jsonObject, "membersSize", membersSize);
    json_object_set_new(jsonObject, "builtin", builtin);
    json_object_set_new(jsonObject, "fixedSize", fixedSize);
    json_object_set_new(jsonObject, "overlayable", overlayable);
    json_object_set_new(jsonObject, "binaryEncodingId", binaryEncodingId);
    json_object_set_new(jsonObject, "members", members);
}

void zkUA_jsonEncode_UA_Variant_setObjectDataAndType(json_t *jsonObject,
        json_t *data, int dataType, char *dataIndex) {

    if (dataIndex == NULL)
        json_object_set_new(jsonObject, "data", data);
    else
        /* In case of arrays, we have to index the data otherwise
         the value corresponding to the "data" key will be rewritten */
        json_object_set_new(jsonObject, dataIndex, data);
    json_object_set_new(jsonObject, "type", json_integer(dataType));

}

void zkUA_jsonEncode_UA_Variant_setValue(const UA_DataType *type,
        void *variantValue, json_t *jsonObject, char *dataIndex) {

    json_t *data;
    /* See page 6 of Part 6 of the OPC UA Spec. R1.03
     for the 23 possible types for a Variant */
    if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        ;
        int vValueInt;
        if (*(UA_Boolean *) variantValue == UA_TRUE)
            vValueInt = 1;
        else
            vValueInt = 0;
        data = json_boolean(vValueInt);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_BOOLEAN, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
        data = json_integer(*(UA_SByte*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_SBYTE, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
        data = json_integer(*(UA_Byte*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_BYTE, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
        data = json_integer(*(UA_Int16*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_INT16, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
        data = json_integer(*(UA_UInt16 *) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_UINT16, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
        data = json_integer(*(UA_Int32*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_INT32, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
        data = json_integer(*(UA_UInt32*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_UINT32, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
        json_int_t iHandle = *(UA_Int64*) variantValue;
        data = json_integer(iHandle);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_INT64, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
        json_int_t iHandle = *(UA_UInt64*) variantValue;
        data = json_integer(iHandle);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_UINT64, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
        data = json_real(*(UA_Float*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_FLOAT, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        json_int_t iHandle = *(UA_Double*) variantValue;
        data = json_real(iHandle);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_DOUBLE, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_STRING]) {
        json_t *data = zkUA_jsonEncode_UA_String((UA_String *) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_STRING, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DATETIME]) {
        json_int_t iHandle = *(UA_DateTime*) variantValue;
        data = json_integer(iHandle);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_DATETIME, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_GUID]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_Guid((UA_Guid *) variantValue, data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_GUID, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
        json_t *data = zkUA_jsonEncode_UA_String((UA_String *) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_BYTESTRING, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_XMLELEMENT]) {
        json_t *data = zkUA_jsonEncode_UA_String((UA_String *) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_XMLELEMENT, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_NODEID]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_NodeId((UA_NodeId *) variantValue, data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_NODEID, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_EXPANDEDNODEID]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_ExpandedNodeId((UA_ExpandedNodeId *) variantValue,
                data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_EXPANDEDNODEID, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_STATUSCODE]) {
        data = json_integer(*(UA_StatusCode*) variantValue);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_STATUSCODE, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_QUALIFIEDNAME]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_QualifiedName((UA_QualifiedName *) variantValue,
                data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_QUALIFIEDNAME, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_LocalizedText((UA_LocalizedText *) variantValue,
                data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_LOCALIZEDTEXT, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_ExtensionObject((UA_ExtensionObject *) variantValue,
                data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_EXTENSIONOBJECT, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DATAVALUE]) {
        json_t *data = json_object();
        zkUA_jsonEncode_UA_DataValue((UA_DataValue *) variantValue, data);
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data,
                UA_TYPES_DATAVALUE, dataIndex);
    } else {
        json_t *data = json_string("UNKNOWN_OR_UNSUPPORTED");
        zkUA_jsonEncode_UA_Variant_setObjectDataAndType(jsonObject, data, 999,
                dataIndex);
    }
}

/*
 * zkUA_jsonEncode_UA_Variant_callSetValueByType:
 * Used to cast the variant data before calling zkUA_jsonEncode_UA_Variant_setValue for 
 *  array variant values
 */
void zkUA_jsonEncode_UA_Variant_callSetValueByType(UA_Variant *variant,
        json_t *jsonObject, int dataIndexInt) {

    const UA_DataType *type = variant->type;
    int j = dataIndexInt;
    char *dataIndex = (char *) calloc(65535, sizeof(char));
    snprintf(dataIndex, 65535, "data[%i]", dataIndexInt);

    if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        ;
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Boolean *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_SByte *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Byte *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Int16 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_UInt16 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Int32 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_UInt32 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Int64 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_UInt64 *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Float *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Double *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_STRING]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_String *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DATETIME]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_DateTime *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_GUID]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_Guid *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_ByteString *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_XMLELEMENT]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_XmlElement *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_NODEID]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_NodeId *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_EXPANDEDNODEID]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_ExpandedNodeId *) variant->data)[j], jsonObject,
                dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_STATUSCODE]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_StatusCode *) variant->data)[j], jsonObject, dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_QUALIFIEDNAME]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_QualifiedName *) variant->data)[j], jsonObject,
                dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_LocalizedText *) variant->data)[j], jsonObject,
                dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_ExtensionObject *) variant->data)[j], jsonObject,
                dataIndex);
    } else if (type == &UA_TYPES[UA_TYPES_DATAVALUE]) {
        zkUA_jsonEncode_UA_Variant_setValue((UA_DataType *) variant->type,
                &((UA_DataValue *) variant->data)[j], jsonObject, dataIndex);
    } else {
        fprintf(stderr,
                "zkUA_jsonEncode_UA_Variant_callSetValueByType: Uknown type\n");
    }
    free(dataIndex);
}

/**
 * zkUA_jsonEncode_UA_Variant:
 * See page 84 of part 6 of the standard
 */
void zkUA_jsonEncode_UA_Variant(UA_Variant *variant, json_t *jsonObject) {

    json_t *storageType;
    if (variant->storageType == UA_VARIANT_DATA) {
        storageType = json_string("UA_VARIANT_DATA");
    } else {
        storageType = json_string("UA_VARIANT_DATA_NODELETE");
    }
    json_t *arrayLength = json_integer(variant->arrayLength);

    json_object_set_new(jsonObject, "storageType", storageType);
    json_object_set_new(jsonObject, "arrayLength", arrayLength);

    size_t length = variant->arrayLength;
    if (length == -1) {
        /* empty variants contain an array of length -1 (undefined). */
        /* do nothing really */
        return;
    }

    /* what data type is stored in this variant? */
    json_t *type = json_object();
    UA_DataType *varType = (UA_DataType *) variant->type;
    zkUA_jsonEncode_UA_DataType(varType, type);
    json_object_set_new(jsonObject, "type", type);

    if (UA_Variant_isScalar(variant)) {
        /* save the scalar data */
        zkUA_jsonEncode_UA_Variant_setValue(varType, variant->data, jsonObject,
                NULL);
    } else { /* array handling */
        /* The number of dimensions*/
        json_t *arrayDimensionsSize = json_integer(
                variant->arrayDimensionsSize);
        json_object_set_new(jsonObject, "arrayDimensionsSize",
                arrayDimensionsSize);

        size_t dims;
        /* If there is only one dimension */
        if (variant->arrayDimensionsSize == 0) {
            UA_UInt32 aD = (UA_UInt32) (uintptr_t) variant->arrayDimensions;
            json_t *arrayDimensions = json_integer(aD);
            json_object_set_new(jsonObject, "arrayDimensions", arrayDimensions);
        } else {
            /* otherwise store the length of each dimension */
            dims = variant->arrayDimensionsSize;
            char *dimBuff = calloc(65535, sizeof(char));
            for (size_t i = 0; i < dims; i++) {
                json_t *arrayDimensions = json_integer(
                        variant->arrayDimensions[i]);
                snprintf(dimBuff, 65535, "arrayDimensions[%lu]", i);
                json_object_set_new(jsonObject, dimBuff, arrayDimensions);
                memset(dimBuff, 0, 65535);
            }
            free(dimBuff);
        }

        /* since, in reality, all of the values are stored in an array, just copy all the data
         sequentially */
        int aLength = variant->arrayLength;
        char *dataIndex = (char *) calloc(65535, sizeof(char));
        for (int j = 0; j < aLength; j++) {
            /* Initialize the JSON Object "key" (data index) string */
            snprintf(dataIndex, 65535, "data[%i]", j);
            zkUA_jsonEncode_UA_Variant_callSetValueByType(variant, jsonObject,
                    j);
            memset(dataIndex, 0, 65535);
        }
        free(dataIndex);
    }
}

void zkUA_jsonEncode_UA_ExtensionObject(UA_ExtensionObject *eObject,
        json_t *jsonObject) {

    json_t *encoding;
    switch ((int) eObject->encoding) {
    case UA_EXTENSIONOBJECT_ENCODED_NOBODY:
    case UA_EXTENSIONOBJECT_ENCODED_BYTESTRING:
    case UA_EXTENSIONOBJECT_ENCODED_XML: {
        if (eObject->encoding == UA_EXTENSIONOBJECT_ENCODED_NOBODY) {
            encoding = json_string("UA_EXTENSIONOBJECT_ENCODED_NOBODY");
        } else if (eObject->encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING) {
            encoding = json_string("UA_EXTENSIONOBJECT_ENCODED_BYTESTRING");
        } else {
            encoding = json_string("UA_EXTENSIONOBJECT_ENCODED_XML");
        }

        json_t *content = json_object();
        json_t *encoded = json_object();
        json_t *typeId = json_object();

        zkUA_jsonEncode_UA_NodeId(&eObject->content.encoded.typeId, typeId);

        /* encode the bytestring */
        json_t *body = zkUA_jsonEncode_UA_String(
                &eObject->content.encoded.body);
        /* place everything inside their respective objects */
        json_object_set_new(jsonObject, "encoding", encoding);
        json_object_set_new(encoded, "typeId", typeId);
        json_object_set_new(encoded, "body", body);
        json_object_set_new(content, "encoded", encoded);
        json_object_set_new(jsonObject, "content", content);
        break;
    }
    case UA_EXTENSIONOBJECT_DECODED:
    case UA_EXTENSIONOBJECT_DECODED_NODELETE: {
        if (eObject->encoding == UA_EXTENSIONOBJECT_DECODED) {
            encoding = json_string("UA_EXTENSIONOBJECT_DECODED");
        } else {
            encoding = json_string("UA_EXTENSIONOBJECT_DECODED_NODELETE");
        }
        json_t *content = json_object();
        json_t *decoded = json_object();
        json_t *type = json_object();
        UA_DataType *typePtr = (UA_DataType *) eObject->content.decoded.type;
        zkUA_jsonEncode_UA_DataType(typePtr, type);
        /* TODO: encode the *data based on the type */
        // use zkUA_jsonEncode_UA_Variant because it can handle scalar/array of different types.
        /* place everything inside their respective objects */
        json_object_set_new(jsonObject, "encoding", encoding);
        json_object_set_new(decoded, "type", type);
        json_object_set_new(content, "decoded", decoded);
        json_object_set_new(jsonObject, "content", content);

        break;
    }
    default: {
        encoding = json_string("UNKNOWN ENCODING");
        json_object_set_new(jsonObject, "encoding", encoding);
        break;
    }
    }
}

void zkUA_jsonEncode_UA_DataValue(UA_DataValue *dataValue, json_t *jsonObject) {

    json_t *hasValue = json_boolean(dataValue->hasValue);
    json_t *hasStatus = json_boolean(dataValue->hasStatus);
    json_t *hasSourceTimestamp = json_boolean(dataValue->hasSourceTimestamp);
    json_t *hasServerTimestamp = json_boolean(dataValue->hasServerTimestamp);
    json_t *hasSourcePicoseconds = json_boolean(
            dataValue->hasSourcePicoseconds);
    json_t *hasServerPicoseconds = json_boolean(
            dataValue->hasServerPicoseconds);

    json_object_set_new(jsonObject, "hasValue", hasValue);
    json_object_set_new(jsonObject, "hasStatus", hasStatus);
    json_object_set_new(jsonObject, "hasSourceTimestamp", hasSourceTimestamp);
    json_object_set_new(jsonObject, "hasServerTimestamp", hasServerTimestamp);
    json_object_set_new(jsonObject, "hasSourcePicoseconds",
            hasSourcePicoseconds);
    json_object_set_new(jsonObject, "hasServerPicoseconds",
            hasServerPicoseconds);

    if (dataValue->hasValue) {
        json_t *value = json_object();
        zkUA_jsonEncode_UA_Variant(&dataValue->value, value);
        json_object_set_new(jsonObject, "value", value);
    }
    if (dataValue->hasStatus) {
        json_t *status = json_integer(dataValue->hasStatus);
        json_object_set_new(jsonObject, "status", status);
    }
    if (dataValue->hasSourceTimestamp) {
        json_t *sourceTimestamp = json_integer(dataValue->sourceTimestamp);
        json_object_set_new(jsonObject, "sourceTimestamp", sourceTimestamp);
    }
    if (dataValue->hasServerTimestamp) {
        json_t *serverTimestamp = json_integer(dataValue->serverTimestamp);
        json_object_set_new(jsonObject, "serverTimestamp", serverTimestamp);
    }
    if (dataValue->hasSourcePicoseconds) {
        json_t *sourcePicoseconds = json_integer(dataValue->sourcePicoseconds);
        json_object_set_new(jsonObject, "sourcePicoseconds", sourcePicoseconds);
    }
    if (dataValue->hasServerPicoseconds) {
        json_t *serverPicoseconds = json_integer(dataValue->serverPicoseconds);
        json_object_set_new(jsonObject, "serverPicoseconds", serverPicoseconds);
    }
}

void zkUA_jsonEncode_UA_DiagnosticInfo(UA_DiagnosticInfo *dInfo,
        json_t *jsonObject) {

    json_t *hasSymbolicId = json_boolean(dInfo->hasSymbolicId);
    json_t *hasNamespaceUri = json_boolean(dInfo->hasNamespaceUri);
    json_t *hasLocalizedText = json_boolean(dInfo->hasLocalizedText);
    json_t *hasLocale = json_boolean(dInfo->hasLocale);
    json_t *hasAdditionalInfo = json_boolean(dInfo->hasAdditionalInfo);
    json_t *hasInnerStatusCode = json_boolean(dInfo->hasInnerStatusCode);
    json_t *hasInnerDiagnosticInfo = json_boolean(
            dInfo->hasInnerDiagnosticInfo);

    json_object_set_new(jsonObject, "hasSymbolicId", hasSymbolicId);
    json_object_set_new(jsonObject, "hasNamespaceUri", hasNamespaceUri);
    json_object_set_new(jsonObject, "hasLocalizedText", hasLocalizedText);
    json_object_set_new(jsonObject, "hasLocale", hasLocale);
    json_object_set_new(jsonObject, "hasAdditionalInfo", hasAdditionalInfo);
    json_object_set_new(jsonObject, "hasInnerStatusCode", hasInnerStatusCode);
    json_object_set_new(jsonObject, "hasInnerDiagnosticInfo",
            hasInnerDiagnosticInfo);

    if (dInfo->hasSymbolicId) {
        json_t *symbolicId = json_integer(dInfo->symbolicId);
        json_object_set_new(jsonObject, "symbolicId", symbolicId);
    }
    if (dInfo->hasNamespaceUri) {
        json_t *namespaceUri = json_integer(dInfo->namespaceUri);
        json_object_set_new(jsonObject, "namespaceUri", namespaceUri);
    }
    if (dInfo->hasLocalizedText) {
        json_t *localizedText = json_integer(dInfo->localizedText);
        json_object_set_new(jsonObject, "localizedText", localizedText);
    }
    if (dInfo->hasLocale) {
        json_t *locale = json_integer(dInfo->locale);
        json_object_set_new(jsonObject, "locale", locale);
    }
    if (dInfo->hasAdditionalInfo) {
        json_t *additionalInfo = zkUA_jsonEncode_UA_String(
                &dInfo->additionalInfo);
        json_object_set_new(jsonObject, "additionalInfo", additionalInfo);
    }
    if (dInfo->hasInnerStatusCode) {
        json_t *innerStatusCode = json_integer(dInfo->innerStatusCode);
        json_object_set_new(jsonObject, "innerStatusCode", innerStatusCode);
    }
    if (dInfo->hasInnerDiagnosticInfo) {
        json_t *innerDiagnosticInfo = json_object();
        zkUA_jsonEncode_UA_DiagnosticInfo(dInfo->innerDiagnosticInfo,
                innerDiagnosticInfo);
        json_object_set_new(jsonObject, "innerDiagnosticInfo",
                innerDiagnosticInfo);
    }
}

void zkUA_jsonEncode_UA_NumericRangeDimension(UA_NumericRangeDimension *nRD,
        json_t *jsonObject) {
    json_t *min = json_integer(nRD->min);
    json_t *max = json_integer(nRD->max);
    json_object_set_new(jsonObject, "min", min);
    json_object_set_new(jsonObject, "max", max);
}

void zkUA_jsonEncode_UA_NumericRange(UA_NumericRange *nRange,
        json_t *jsonObject) {

    size_t dims = nRange->dimensionsSize;
    json_t *dimensionsSize = json_integer(dims);
    json_object_set_new(jsonObject, "dimensionsSize", dimensionsSize);

    /* cycle through all of the dimensions' min max ranges */
    for (size_t i = 0; i < dims; i++) {
        json_t *dim = json_object();
        zkUA_jsonEncode_UA_NumericRangeDimension(&nRange->dimensions[i], dim);
        char *buff = calloc(65535, sizeof(char));
        snprintf(buff, 65535, "dimensions[%lu]", i);
        json_object_set_new(jsonObject, buff, dim);
        free(buff);
    }
}

void zkUA_jsonEncode_UA_StatusCodeDescription(
        UA_StatusCodeDescription *sCDescrip, json_t *jsonObject) {

    json_t *code = json_integer(sCDescrip->code);
    json_t *name = json_string(sCDescrip->name);
    json_t *explanation = json_string(sCDescrip->explanation);

    json_object_set_new(jsonObject, "code", code);
    json_object_set_new(jsonObject, "name", name);
    json_object_set_new(jsonObject, "explanation", explanation);
}

void zkUA_jsonEncode_UA_Guid(UA_Guid *guid, json_t *jsonObject) {

    /* exact copy of the contents of the struct */
    json_t *data1 = json_integer(guid->data1);
    json_t *data2 = json_integer(guid->data2);
    json_t *data3 = json_integer(guid->data3);
    json_t *data4 = json_array();

    json_array_append_new(data4, json_integer(guid->data4[0]));
    json_array_append_new(data4, json_integer(guid->data4[1]));
    json_array_append_new(data4, json_integer(guid->data4[2]));
    json_array_append_new(data4, json_integer(guid->data4[3]));
    json_array_append_new(data4, json_integer(guid->data4[4]));
    json_array_append_new(data4, json_integer(guid->data4[5]));
    json_array_append_new(data4, json_integer(guid->data4[6]));
    json_array_append_new(data4, json_integer(guid->data4[7]));

    /* this is only for aesthetic reasons and is not actually part
     of the struct */
    char *guidStringBuffer = calloc(65535, sizeof(char));
    snprintf(guidStringBuffer, 65535, UA_PRINTF_GUID_FORMAT,
            UA_PRINTF_GUID_DATA(*guid));
    json_t *guidString = json_string(guidStringBuffer);
    free(guidStringBuffer);

    /* set object with encoded values */
    json_object_set_new(jsonObject, "data1", data1);
    json_object_set_new(jsonObject, "data2", data2);
    json_object_set_new(jsonObject, "data3", data3);
    json_object_set_new(jsonObject, "data4", data4);
    /* add the human-readable form of the guid */
    json_object_set_new(jsonObject, "guidString", guidString);
}

/**
 * zkUA_jsonEncode_UA_DateTimeStruct:
 * Encodes UA_DateTimeStruct into a given JSON object
 */
void zkUA_jsonEncode_UA_DateTimeStruct(UA_DateTimeStruct *dtStruct,
        json_t *jsonObject) {

    json_t *nanoSec = json_integer(dtStruct->nanoSec);
    json_t *microSec = json_integer(dtStruct->microSec);
    json_t *milliSec = json_integer(dtStruct->milliSec);
    json_t *sec = json_integer(dtStruct->sec);
    json_t *min = json_integer(dtStruct->min);
    json_t *hour = json_integer(dtStruct->hour);
    json_t *day = json_integer(dtStruct->day);
    json_t *month = json_integer(dtStruct->month);
    json_t *year = json_integer(dtStruct->year);

    json_object_set_new(jsonObject, "nanoSec", nanoSec);
    json_object_set_new(jsonObject, "microSec", microSec);
    json_object_set_new(jsonObject, "milliSec", milliSec);
    json_object_set_new(jsonObject, "sec", sec);
    json_object_set_new(jsonObject, "min", min);
    json_object_set_new(jsonObject, "hour", hour);
    json_object_set_new(jsonObject, "day", day);
    json_object_set_new(jsonObject, "month", month);
    json_object_set_new(jsonObject, "year", year);
}

/**
 * zkUA_jsonEncode_UA_String:
 * Encodes UA_String into a JSON object and returns the object
 */
json_t *zkUA_jsonEncode_UA_String(UA_String *uaString) {

    char *buffer = (char *) calloc(65535, sizeof(char));
    snprintf(buffer, 65535, "%.*s", (int) uaString->length, uaString->data);
    json_t *jsonString = json_string(buffer);
    free(buffer);
    return jsonString;
}
/**
 * zkUA_jsonEncode_UA_LocalizedText:
 * Encodes a UA_LocalizedText into the provided JSON object
 */
void zkUA_jsonEncode_UA_LocalizedText(UA_LocalizedText *lText,
        json_t *jsonObject) {

    json_t *locale = json_object();
    json_t *locale_length = json_integer(lText->locale.length);
    char *localeBuffer = (char *) calloc(65535, sizeof(char));
    snprintf(localeBuffer, 65535, "%.*s", (int) lText->locale.length,
            lText->locale.data);
    json_t *locale_data = json_string(localeBuffer);
    free(localeBuffer);
    json_object_set_new(locale, "length", locale_length);
    json_object_set_new(locale, "data", locale_data);

    json_t *text = json_object();
    json_t *text_length = json_integer(lText->text.length);
    char *textBuffer = (char *) calloc(65535, sizeof(char));
    snprintf(textBuffer, 65535, "%.*s", (int) lText->text.length,
            lText->text.data);
    json_t *text_data = json_string(textBuffer);
    free(textBuffer);
    json_object_set_new(text, "length", text_length);
    json_object_set_new(text, "data", text_data);

    json_object_set_new(jsonObject, "locale", locale);
    json_object_set_new(jsonObject, "text", text);
}

/**
 * zkUA_jsonEncode_UA_NodeId:
 * Encodes a UA_NodeID into the provided JSON object
 */
UA_StatusCode zkUA_jsonEncode_UA_NodeId(UA_NodeId *nodeId, json_t *jsonObject) {

    json_t *nodeId_ns = json_integer(nodeId->namespaceIndex);
    json_t *nodeId_idType = json_integer(nodeId->identifierType);
    json_object_set_new(jsonObject, "namespaceIndex", nodeId_ns);
    json_object_set_new(jsonObject, "identifierType", nodeId_idType);

    json_t *identifierTypeString;
    if (nodeId->identifierType == UA_NODEIDTYPE_NUMERIC) {
        json_t *nodeId_id = json_integer(nodeId->identifier.numeric);
        json_object_set_new(jsonObject, "identifier", nodeId_id);
        /* For human-readability of the NodeIdType */
        identifierTypeString = json_string("UA_NODEIDTYPE_NUMERIC");
        json_object_set_new(jsonObject, "identifierTypeString",
                identifierTypeString);
    } else if (nodeId->identifierType == UA_NODEIDTYPE_STRING) {
        json_t *nodeId_id = zkUA_jsonEncode_UA_String(
                &nodeId->identifier.string);
        json_object_set_new(jsonObject, "identifier", nodeId_id);
        identifierTypeString = json_string("UA_NODEIDTYPE_STRING");
        json_object_set_new(jsonObject, "identifierTypeString",
                identifierTypeString);
    } /* TODO: distinguish further types */
    else if (nodeId->identifierType == UA_NODEIDTYPE_GUID) {
        fprintf(stderr, "Node ID Type GUID\n");
        json_t *nodeId_id = json_object();
        zkUA_jsonEncode_UA_Guid(&nodeId->identifier.guid, nodeId_id);
        json_object_set_new(jsonObject, "identifier", nodeId_id);
        identifierTypeString = json_string("UA_NODEIDTYPE_GUID");
        json_object_set_new(jsonObject, "identifierTypeString",
                identifierTypeString);
    } else if (nodeId->identifierType == UA_NODEIDTYPE_BYTESTRING) {
        json_t *nodeId_id = zkUA_jsonEncode_UA_String(
                &nodeId->identifier.byteString);
        json_object_set_new(jsonObject, "identifier", nodeId_id);
        identifierTypeString = json_string("UA_NODEIDTYPE_BYTESTRING");
        json_object_set_new(jsonObject, "identifierTypeString",
                identifierTypeString);
    } else {
        fprintf(stderr,
                "zkUA_jsonEncode_UA_NodeId: Error! Unknown NodeID Type: %d for ns=%d id=%u\n",
                nodeId->identifierType, nodeId->namespaceIndex,
                nodeId->identifier.numeric /* :p */);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return UA_STATUSCODE_GOOD;
}

/**
 * zkUA_jsonEncode_UA_ExpandedNodeId:
 * Encodes a UA_ExpandedNodeId into the provided JSON object
 */
void zkUA_jsonEncode_UA_ExpandedNodeId(UA_ExpandedNodeId *expId,
        json_t *jsonObject) {

    json_t *expId_nodeId = json_object();
    zkUA_jsonEncode_UA_NodeId(&expId->nodeId, expId_nodeId);
    json_t *namespaceUri = zkUA_jsonEncode_UA_String(&expId->namespaceUri);
    json_t *serverIndex = json_integer(expId->serverIndex);
    json_object_set_new(jsonObject, "nodeId", expId_nodeId);
    json_object_set_new(jsonObject, "namespaceUri", namespaceUri);
    json_object_set_new(jsonObject, "serverIndex", serverIndex);
}

/**
 * zkUA_jsonEncode_UA_QualifiedName:
 * Encodes a UA_QualifiedName into the provided JSON object
 */
void zkUA_jsonEncode_UA_QualifiedName(UA_QualifiedName *qualName,
        json_t *jsonObject) {

    json_t *browseName_namespaceIndex = json_integer(qualName->namespaceIndex);
    json_t *browseName_name = zkUA_jsonEncode_UA_String(&qualName->name);
    json_object_set_new(jsonObject, "namespaceIndex",
            browseName_namespaceIndex);
    json_object_set_new(jsonObject, "name", browseName_name);
}

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
        json_t *jsonObject) {

    json_object_set_new(jsonObject, "referenceTypeId", referenceTypeId);
    json_object_set_new(jsonObject, "isForward", isForward);
    json_object_set_new(jsonObject, "nodeId", nodeId);
    json_object_set_new(jsonObject, "browseName", browseName);
    json_object_set_new(jsonObject, "displayName", displayName);
    json_object_set_new(jsonObject, "nodeClass", nodeClass);
    json_object_set_new(jsonObject, "typeDefinition", typeDefinition);

}

/**
 * zkUA_jsonEncodeAttributes:
 * Given all of the attributes of a node, this
 * function encodes all said attributes into the
 * provided JSON object.
 */
void zkUA_jsonEncodeAttributes(UA_ReferenceDescription *childRef,
        void *nodeAttributes, json_t *jsonAtt) {

    /* NodeClass */
    UA_NodeClass nodeClass = childRef->nodeClass;
    if (nodeClass == UA_NODECLASS_UNSPECIFIED) {
        fprintf(stderr,
                "zkUA_jsonEncodeAttributes: Error! nodeclass is UA_NODECLASS_UNSPECIFIED. Returning...\n");
        return;
    }
    /* Common node attributes that apply to all node classes */
    UA_ObjectAttributes *attributes = (UA_ObjectAttributes *) nodeAttributes;

    /* create root JSON object */
    json_t *jsonAttributes = jsonAtt;

    /* Display Name attribute */
    json_t *displayName = json_object();
    zkUA_jsonEncode_UA_LocalizedText(&attributes->displayName, displayName);

    /* Description attribute */
    json_t *description = json_object();
    zkUA_jsonEncode_UA_LocalizedText(&attributes->description, description);

    json_t *writeMask = json_integer(attributes->writeMask);

    json_object_set_new(jsonAttributes, "displayName", displayName);
    json_object_set_new(jsonAttributes, "description", description);
    json_object_set_new(jsonAttributes, "writeMask", writeMask);

    /* only the object and view classes have the eventNotifier attribute */
    if (nodeClass == (UA_NODECLASS_OBJECT || UA_NODECLASS_VIEW)) {
        json_t *eventNotifier = json_integer(attributes->eventNotifier);
        json_object_set_new(jsonAttributes, "eventNotifier", eventNotifier);
    }

    /* Attributes differ based on the node class */
    switch (nodeClass) {
    case UA_NODECLASS_VIEW: {
        /* attributes specific to view */
        UA_ViewAttributes *viewAttributes = (UA_ViewAttributes *) nodeAttributes;
        json_t *containsNoLoops = json_integer(viewAttributes->containsNoLoops);
        json_object_set_new(jsonAttributes, "containsNoLoops", containsNoLoops);
        break;
    }
    case UA_NODECLASS_VARIABLE:
    case UA_NODECLASS_VARIABLETYPE: {
        /* attributes specific to variable and variabletype -
         note that you should be using a union struct */
        UA_VariableAttributes *varAttributes =
                (UA_VariableAttributes *) nodeAttributes;
        json_t *value;
        /* check if the variant is empty */
        const UA_Variant *testValue = &varAttributes->value;
        if (UA_Variant_isEmpty(testValue)) {
            value = json_string("empty");
            /* note that the pointer to the type description is NULL when the variant is empty*/
        } else {
            value = json_object();
            zkUA_jsonEncode_UA_Variant(&varAttributes->value, value);
        }

        json_t *dataType = json_object();
        zkUA_jsonEncode_UA_NodeId(&varAttributes->dataType, dataType);

        json_t *valueRank = json_integer(varAttributes->valueRank);
        json_t *arrayDimensionsSize = json_integer(
                varAttributes->arrayDimensionsSize);

        json_object_set_new(jsonAttributes, "value", value);
        json_object_set_new(jsonAttributes, "dataType", dataType);
        json_object_set_new(jsonAttributes, "valueRank", valueRank);
        json_object_set_new(jsonAttributes, "arrayDimensionsSize",
                arrayDimensionsSize);

        if (nodeClass == UA_NODECLASS_VARIABLE) {
            json_t *accessLevel = json_integer(varAttributes->accessLevel);
            json_t *userAccessLevel = json_integer(
                    varAttributes->userAccessLevel);
            json_t *minimumSamplingInterval = json_integer(
                    varAttributes->minimumSamplingInterval);

            json_object_set_new(jsonAttributes, "accessLevel", accessLevel);
            json_object_set_new(jsonAttributes, "userAccessLevel",
                    userAccessLevel);
            json_object_set_new(jsonAttributes, "minimumSamplingInterval",
                    minimumSamplingInterval);

            json_t *historizing = json_boolean(varAttributes->historizing);
            json_object_set_new(jsonAttributes, "historizing", historizing);
        } else {
            UA_VariableTypeAttributes *varTAttributes =
                    (UA_VariableTypeAttributes *) nodeAttributes;
            json_t *isAbstract = json_boolean(varTAttributes->isAbstract);
            json_object_set_new(jsonAttributes, "isAbstract", isAbstract);
        }
        break;
    }
    case UA_NODECLASS_REFERENCETYPE: {
        /* attributes specific to referencetype*/
        UA_ReferenceTypeAttributes * refAttributes =
                (UA_ReferenceTypeAttributes *) nodeAttributes;
        json_t *isAbstract = json_boolean(refAttributes->isAbstract);
        json_t *symmetric = json_boolean(refAttributes->symmetric);

        /* inverseName attribute */
        json_t *inverseName = json_object();
        zkUA_jsonEncode_UA_LocalizedText(&refAttributes->inverseName,
                inverseName);

        json_object_set_new(jsonAttributes, "isAbstract", isAbstract);
        json_object_set_new(jsonAttributes, "symmetric", symmetric);
        json_object_set_new(jsonAttributes, "inverseName", inverseName);
        break;
    }
    case UA_NODECLASS_OBJECTTYPE: {
        UA_ObjectTypeAttributes * objA =
                (UA_ObjectTypeAttributes *) nodeAttributes;
        json_t *isAbstract = json_boolean(objA->isAbstract);
        json_object_set_new(jsonAttributes, "isAbstract", isAbstract);
        break;
    }
    case UA_NODECLASS_DATATYPE: {
        /* attributes specific to datatype/objecttype */
        UA_DataTypeAttributes * dataA = (UA_DataTypeAttributes *) nodeAttributes;
        json_t *isAbstract = json_boolean(dataA->isAbstract);
        json_object_set_new(jsonAttributes, "isAbstract", isAbstract);
        break;
    }
    case UA_NODECLASS_METHOD: {
        /* attributes specific to method */
        UA_MethodAttributes * methodA = (UA_MethodAttributes *) nodeAttributes;
        json_t *executable = json_boolean(methodA->executable);
        json_object_set_new(jsonAttributes, "executable", executable);
        break;
    }
    default: {
        if (nodeClass != UA_NODECLASS_OBJECT)
            fprintf(stderr,
                    "zkUA_jsonEncode_UA_Attributes: Didn't match any nodeClass\n");
        break;
    }
    }
}
