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
#include <zk_jsonDecode.h>
#include <zk_serverReplicate.h>
#include <zk_intercept.h>
/* For debugging purposes */
#include <simple_parse.h>
#include <zk_global.h>
/* Declare variables */
UA_Server *uaServer = NULL;
/** JSON Decoding Functions **/

UA_StatusCode zkUA_jsonDecode_UA_LocalizedText(json_t *localizedText,
        UA_LocalizedText *destination) {

    char *lString, *tString;

    /* Extract the locale object and its string formatted contents and string length */
    json_t *locale = json_object_get(localizedText, "locale");
    if (!json_is_object(locale)) {
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *data = json_object_get(locale, "data");
    lString = zkUA_jsonDecode_UA_String(data);

    /* Extract the text object and its string formatted contents and string length */
    json_t *text = json_object_get(localizedText, "text");
    if (!json_is_object(text)) {
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    data = json_object_get(text, "data");
    tString = zkUA_jsonDecode_UA_String(data);

    /* Fill in the given UA_LocalizedText struct with the locale and text */
    UA_LocalizedText tmpStore = UA_LOCALIZEDTEXT(lString, tString);
    destination->locale.length = tmpStore.locale.length;
    if (tmpStore.locale.length != 0) {
        destination->locale.data = malloc(tmpStore.locale.length);
        memcpy(destination->locale.data, tmpStore.locale.data,
                tmpStore.locale.length);
    }
    destination->text.length = tmpStore.text.length;
    if (tmpStore.text.length != 0) {
        destination->text.data = malloc(tmpStore.text.length);
        memcpy(destination->text.data, tmpStore.text.data,
                tmpStore.text.length);
    }
    UA_LocalizedText_deleteMembers(&tmpStore);
    return UA_STATUSCODE_GOOD;
}

char *zkUA_jsonDecode_UA_String(json_t *string) {

    char *dataString = calloc(65535, sizeof(char));
    snprintf(dataString, 65535, "%s", json_string_value(string));
    return dataString;
}

UA_StatusCode zkUA_jsonDecode_UA_Guid(json_t *guid, UA_Guid *g) {

    /* Initialize memory for guid */
    const char *key;
    json_t *value;

    json_object_foreach(guid, key, value)
    {
        if (strcmp(key, "data1") == 0)
            json_unpack(value, "i", &g->data1);
        if (strcmp(key, "data2") == 0)
            json_unpack(value, "i", &g->data2);
        if (strcmp(key, "data3") == 0)
            json_unpack(value, "i", &g->data3);
        if (strcmp(key, "data4") == 0) {
            /* unpack the byte array */
            size_t size = json_array_size(value);
            size_t tmp;
            int sInt;
            for (size_t i = 0; i < size; i++) {
                sInt = json_unpack(json_array_get(value, i), "i", &tmp);
                if (sInt == -1) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_UA_Guid: Unable to unpack data4 array element %lu\n",
                            i);
                    return UA_STATUSCODE_BADUNEXPECTEDERROR;
                }
                g->data4[i] = (UA_Byte) tmp;
            }
        }
    }

    return UA_STATUSCODE_GOOD;
}

/**
 * zkUA_jsonDecode_NodeId:
 * Decodes a JSON object containing a UA_NodeId structure
 */
UA_StatusCode zkUA_jsonDecode_UA_NodeId(json_t *nodeId, UA_NodeId *uaNodeId) {

    /* Note, we're not checking if NodeType String and NodeType ByteString are successful */
    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    int ns, idType, sInt;
    json_t *namespaceIndex = json_object_get(nodeId, "namespaceIndex");
    json_t *identifierType = json_object_get(nodeId, "identifierType");
    json_t *identifier = json_object_get(nodeId, "identifier");
    json_unpack(namespaceIndex, "I", &ns);
    json_unpack(identifierType, "I", &idType);
    switch (idType) {
    case UA_NODEIDTYPE_NUMERIC: {
        sInt = json_unpack(identifier, "I", &uaNodeId->identifier.numeric);
        if (sInt != 0) {
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        break;
    }
    case UA_NODEIDTYPE_STRING: {
        uaNodeId->identifier.string = UA_STRING(
                zkUA_jsonDecode_UA_String(identifier));
        fprintf(stderr, "zkUA_jsonDecode_UA_NodeId: identifier.string %.*s\n",
                (int) uaNodeId->identifier.string.length,
                uaNodeId->identifier.string.data);
        break;
    }
    case UA_NODEIDTYPE_GUID: {
        sCode = zkUA_jsonDecode_UA_Guid(identifier, &uaNodeId->identifier.guid);
        break;
    }
    case UA_NODEIDTYPE_BYTESTRING: {
        uaNodeId->identifier.byteString = UA_STRING(
                zkUA_jsonDecode_UA_String(identifier));
        break;
    }
    default:
        fprintf(stderr,
                "zkUA_jsonDecode_UA_NodeId: Error: Unknown NODIDTYPE.\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    uaNodeId->namespaceIndex = ns;
    uaNodeId->identifierType = idType;
    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_ExpandedNodeId(json_t *jsonObject,
        UA_ExpandedNodeId *eNodeId) {

    UA_StatusCode sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
    const char *key;
    json_t *value;

    /* TODO: add a check to ensure all three keys are found */
    json_object_foreach(jsonObject, key, value)
    {
        if (strcmp(key, "nodeId") == 0)
            sCode = zkUA_jsonDecode_UA_NodeId(value, &eNodeId->nodeId);
        if (strcmp(key, "namespaceUri") == 0) {
            /* Use a temporary store to be able to free the calloc'd string
             generated using jsonDecode_UA_String */
            UA_String tmpStore = UA_STRING(zkUA_jsonDecode_UA_String(value));
            eNodeId->namespaceUri.length = tmpStore.length;
            if (tmpStore.length != 0) {
                eNodeId->namespaceUri.data = malloc(tmpStore.length);
                memcpy(eNodeId->namespaceUri.data, tmpStore.data,
                        tmpStore.length);
            }
            /* Delete the temporary string created by UA_String */
            UA_String_deleteMembers(&tmpStore);
        }
        if (strcmp(key, "serverIndex") == 0)
            json_unpack(value, "i", &eNodeId->serverIndex);
    }

    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_QualifiedName(json_t *jsonObject,
        UA_QualifiedName *qName) {

    const char *key;
    json_t *value;

    json_object_foreach(jsonObject, key, value)
    {
        if (strcmp(key, "namespaceIndex") == 0) {
            if (json_unpack(value, "i", &qName->namespaceIndex) == -1) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_QualifiedName: Failed to unpack the namespace Index\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
        } else if (strcmp(key, "name") == 0) {
            UA_String tmpStore = UA_STRING(zkUA_jsonDecode_UA_String(value));
            qName->name.length = tmpStore.length;
            /* If the string is empty don't malloc 0 to avoid having problems with memory free'ing later */
            if (tmpStore.length != 0) {
                qName->name.data = malloc(tmpStore.length);
                memcpy(qName->name.data, tmpStore.data, tmpStore.length);
            }
            UA_String_deleteMembers(&tmpStore);
        }
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode zkUA_jsonDecode_UA_ExtensionObject_Decoded(json_t *jsonObject,
        UA_ExtensionObject *eObject) {

    int sInt, tIint;
    const char *key;
    json_t *value;

    json_t *content = json_object_get(jsonObject, "content");
    if (!json_is_object(content)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the content object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *decoded = json_object_get(content, "decoded");
    if (!json_is_object(decoded)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the decoded object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *type = json_object_get(decoded, "type");
    if (!json_is_object(type)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the type object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    json_object_foreach(type, key, value)
    {
        if (strcmp(key, "typeIndex") == 0) {
            sInt = json_unpack(value, "i", &tIint);
            if (sInt != 0) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not unpack the typeIndex object\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
        }
    }

    eObject->content.decoded.type = &UA_TYPES[tIint];
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode zkUA_jsonDecode_UA_ExtensionObject_decodeString(
        json_t *jsonObject, UA_ExtensionObject *eObject) {
    UA_StatusCode sCode;

    json_t *content = json_object_get(jsonObject, "content");
    if (!json_is_object(content)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the content object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *encoded = json_object_get(content, "encoded");
    if (!json_is_object(encoded)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the encoded object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *body = json_object_get(encoded, "body");
    if (!json_is_object(body)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the body object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    json_t *typeId = json_object_get(encoded, "typeId");
    if (!json_is_object(typeId)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_Decoded: Could not retrieve the typeId object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    eObject->content.encoded.body = UA_STRING(zkUA_jsonDecode_UA_String(body));
    sCode = zkUA_jsonDecode_UA_NodeId(typeId, &eObject->content.encoded.typeId);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject_decodeString: Could not decode UA_NodeId\n");
    }
    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_ExtensionObject(json_t *jsonObject,
        UA_ExtensionObject *eObject) {

    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    json_t *encoding = json_object_get(jsonObject, "encoding");
    char *encodingChar;
    encodingChar = (char *) json_string_value(encoding);
    if (strcmp(encodingChar, "UA_EXTENSIONOBJECT_ENCODED_NOBODY") == 0) {
        eObject->encoding = UA_EXTENSIONOBJECT_ENCODED_NOBODY;

        UA_NodeId typeId;
        UA_NodeId_init(&typeId);

        eObject->content.encoded.typeId = typeId;
        eObject->content.encoded.body = UA_BYTESTRING_NULL;
    } else if (strcmp(encodingChar, "UA_EXTENSIONOBJECT_ENCODED_BYTESTRING")
            == 0) {
        eObject->encoding = UA_EXTENSIONOBJECT_ENCODED_BYTESTRING;
        sCode = zkUA_jsonDecode_UA_ExtensionObject_decodeString(jsonObject,
                eObject);
    } else if (strcmp(encodingChar, "UA_EXTENSIONOBJECT_ENCODED_XML") == 0) {
        eObject->encoding = UA_EXTENSIONOBJECT_ENCODED_XML;
        sCode = zkUA_jsonDecode_UA_ExtensionObject_decodeString(jsonObject,
                eObject);
    } else if (strcmp(encodingChar, "UA_EXTENSIONOBJECT_DECODED") == 0) {
        eObject->encoding = UA_EXTENSIONOBJECT_ENCODED_XML;
        sCode = zkUA_jsonDecode_UA_ExtensionObject_Decoded(jsonObject, eObject);
    } else if (strcmp(encodingChar, "UA_EXTENSIONOBJECT_DECODED_NODELETE")
            == 0) {
        eObject->encoding = UA_EXTENSIONOBJECT_ENCODED_XML;
        sCode = zkUA_jsonDecode_UA_ExtensionObject_Decoded(jsonObject, eObject);
    } else {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_ExtensionObject: Error! Uknown encoding type\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return sCode;

}

UA_StatusCode zkUA_jsonDecode_UA_DataValue(json_t *jsonObject,
        UA_DataValue *dValue) {

    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    json_t *hasValue = json_object_get(jsonObject, "hasValue");
    json_t *hasStatus = json_object_get(jsonObject, "hasStatus");
    json_t *hasSourceTimestamp = json_object_get(jsonObject,
            "hasSourceTimestamp");
    json_t *hasServerTimestamp = json_object_get(jsonObject,
            "hasServerTimestamp");
    json_t *hasSourcePicoseconds = json_object_get(jsonObject,
            "hasSourcePicoseconds");
    json_t *hasServerPicoseconds = json_object_get(jsonObject,
            "hasServerPicoseconds");

    int boolStore, sInt;
    sInt = json_unpack(hasValue, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasValue = UA_TRUE;
    else
        dValue->hasValue = UA_FALSE;
    sInt = json_unpack(hasStatus, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasStatus = UA_TRUE;
    else
        dValue->hasStatus = UA_FALSE;
    sInt = json_unpack(hasSourceTimestamp, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasSourceTimestamp = UA_TRUE;
    else
        dValue->hasSourceTimestamp = UA_FALSE;
    sInt = json_unpack(hasServerTimestamp, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasServerTimestamp = UA_TRUE;
    else
        dValue->hasServerTimestamp = UA_FALSE;
    sInt = json_unpack(hasSourcePicoseconds, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasSourcePicoseconds = UA_TRUE;
    else
        dValue->hasSourcePicoseconds = UA_FALSE;
    sInt = json_unpack(hasServerPicoseconds, "b", &boolStore);
    if (sInt != 0)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    if (boolStore)
        dValue->hasServerPicoseconds = UA_TRUE;
    else
        dValue->hasServerPicoseconds = UA_FALSE;

    if (dValue->hasValue) {
        json_t *value = json_object_get(jsonObject, "value");
        if (!json_is_object(value))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sCode = zkUA_jsonDecode_UA_Variant(value, &dValue->value);
    }
    if (dValue->hasStatus) {
        json_t *status = json_object_get(jsonObject, "status");
        if (!json_is_object(status))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sInt = json_unpack(status, "i", &dValue->status);
        if (sInt != 0)
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (dValue->hasSourceTimestamp) {
        json_t *sourceTimestamp = json_object_get(jsonObject,
                "sourceTimestamp");
        if (!json_is_object(sourceTimestamp))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sInt = json_unpack(sourceTimestamp, "I", &dValue->sourceTimestamp);
        if (sInt != 0)
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (dValue->hasServerTimestamp) {
        json_t *serverTimestamp = json_object_get(jsonObject,
                "serverTimestamp");
        if (!json_is_object(serverTimestamp))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sInt = json_unpack(serverTimestamp, "I", &dValue->serverTimestamp);
        if (sInt != 0)
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (dValue->hasSourcePicoseconds) {
        json_t *sourcePicoseconds = json_object_get(jsonObject,
                "sourcePicoseconds");
        if (!json_is_object(sourcePicoseconds))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sInt = json_unpack(sourcePicoseconds, "i", &dValue->sourcePicoseconds);
        if (sInt != 0)
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (dValue->hasServerPicoseconds) {
        json_t *serverPicoseconds = json_object_get(jsonObject,
                "serverPicoseconds");
        if (!json_is_object(serverPicoseconds))
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        sInt = json_unpack(serverPicoseconds, "i", &dValue->serverPicoseconds);
        if (sInt != 0)
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_Variant_setData(void *data,
        const UA_DataType *type, json_t *dataValue) {

    UA_StatusCode sCode = UA_STATUSCODE_GOOD;
    int sInt = 0;

    if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        ;
        UA_Boolean_init((UA_Boolean *) data);
        int dataInt;
        sInt = json_unpack(dataValue, "b", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Boolean\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else if (dataInt == 0)
            *(UA_Boolean *) data = UA_FALSE;
        else
            *(UA_Boolean *) data = UA_TRUE;
    } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
        UA_SByte_init(data);
        int dataInt;
        sInt = json_unpack(dataValue, "i", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack SByte\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_SByte *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
        UA_Byte_init(data);
        int dataInt;
        sInt = json_unpack(dataValue, "i", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Byte\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_Byte *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
        UA_Int16_init(data);
        int dataInt;
        sInt = json_unpack(dataValue, "i", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Int16\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_Int16 *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
        UA_UInt16_init(data);
        int dataInt;
        sInt = json_unpack(dataValue, "i", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack UInt16\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_UInt16 *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
        UA_Int32_init((UA_Int32 *) data);
        UA_Int32 dataInt;
        sInt = json_unpack(dataValue, "i", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Int32\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_Int32 *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
        UA_UInt32_init(data);
        sInt = json_unpack(dataValue, "i", data);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack UInt32\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
        UA_Int64_init(data);
        sInt = json_unpack(dataValue, "I", data);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Int64\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
        UA_UInt64_init(data);
        sInt = json_unpack(dataValue, "I", data);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack UInt64\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
        UA_Float_init(data);
        long long int dataInt;
        sInt = json_unpack(dataValue, "f", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Float\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_Float *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        UA_Double_init(data);
        long long int dataInt;
        sInt = json_unpack(dataValue, "f", &dataInt);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack Double\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else
            *(UA_Double *) data = dataInt;
    } else if (type == &UA_TYPES[UA_TYPES_STRING]) {
        UA_String *tmpData = (UA_String *) data;
        UA_String tmpStore = UA_STRING(zkUA_jsonDecode_UA_String(dataValue));
        tmpData->length = tmpStore.length;
        if (tmpStore.length != 0) {
            tmpData->data = malloc(tmpStore.length);
            memcpy(tmpData->data, tmpStore.data, tmpStore.length);
        }
        UA_String_deleteMembers(&tmpStore);
    } else if (type == &UA_TYPES[UA_TYPES_DATETIME]) {
        UA_DateTime_init(data);
        sInt = json_unpack(dataValue, "I", data);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack DateTime\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    } else if (type == &UA_TYPES[UA_TYPES_GUID]) {
        UA_Guid_init(data);
        sCode = zkUA_jsonDecode_UA_Guid(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode Guid\n");
    } else if (type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
        UA_ByteString_init(data);
        UA_ByteString *tmpData = (UA_ByteString *) data;
        UA_String tmpStore = UA_STRING(zkUA_jsonDecode_UA_String(dataValue));
        tmpData->length = tmpStore.length;
        if (tmpStore.length != 0) {
            tmpData->data = malloc(tmpStore.length);
            memcpy(tmpData->data, tmpStore.data, tmpStore.length);
        }
        UA_String_deleteMembers(&tmpStore);
    } else if (type == &UA_TYPES[UA_TYPES_XMLELEMENT]) {
        UA_XmlElement_init(data);
        UA_XmlElement *tmpData = (UA_XmlElement *) data;
        UA_String tmpStore = UA_STRING(zkUA_jsonDecode_UA_String(dataValue));
        tmpData->length = tmpStore.length;
        if (tmpStore.length != 0) {
            tmpData->data = malloc(tmpStore.length);
            memcpy(tmpData->data, tmpStore.data, tmpStore.length);
        }
        UA_String_deleteMembers(&tmpStore);
    } else if (type == &UA_TYPES[UA_TYPES_NODEID]) {
        UA_NodeId_init(data);
        sCode = zkUA_jsonDecode_UA_NodeId(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode NodeId\n");
    } else if (type == &UA_TYPES[UA_TYPES_EXPANDEDNODEID]) {
        UA_ExpandedNodeId_init(data);
        sCode = zkUA_jsonDecode_UA_ExpandedNodeId(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode ExpandedNodeId\n");
    } else if (type == &UA_TYPES[UA_TYPES_STATUSCODE]) {
        UA_StatusCode_init(data);
        sInt = json_unpack(dataValue, "i", data);
        if (sInt != 0) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to unpack StatusCode\n");
            sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    } else if (type == &UA_TYPES[UA_TYPES_QUALIFIEDNAME]) {
        UA_QualifiedName_init(data);
        sCode = zkUA_jsonDecode_UA_QualifiedName(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode QualifiedName\n");
    } else if (type == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
        UA_LocalizedText_init(data);
        sCode = zkUA_jsonDecode_UA_LocalizedText(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode LocalizedText\n");
    } else if (type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
        UA_ExtensionObject_init(data);
        sCode = zkUA_jsonDecode_UA_ExtensionObject(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode ExtensionObject\n");
    } else if (type == &UA_TYPES[UA_TYPES_DATAVALUE]) {
        UA_DataValue_init(data);
        sCode = zkUA_jsonDecode_UA_DataValue(dataValue, data);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant_setData: Failed to decode DataValue\n");
    }

    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_Variant_callSetDataByType(UA_Variant *variant,
        UA_DataType *type, json_t *dataValue, int dataIndex) {

    int i = -1;
    if (dataIndex != -1)
        i = dataIndex;
    UA_StatusCode sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
    void *data = variant->data;
    if (!data)
        fprintf(stderr, " ");

    if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Boolean *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Boolean_new(),
                    &UA_TYPES[UA_TYPES_BOOLEAN]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Boolean *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_SByte *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_SByte_new(),
                    &UA_TYPES[UA_TYPES_SBYTE]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_SByte *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Byte *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Byte_new(),
                    &UA_TYPES[UA_TYPES_BYTE]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Byte *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Int16 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Int16_new(),
                    &UA_TYPES[UA_TYPES_INT16]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Int16 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_UInt16 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_UInt16_new(),
                    &UA_TYPES[UA_TYPES_UINT16]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_UInt16 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Int32 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Int32_new(),
                    &UA_TYPES[UA_TYPES_INT32]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Int32 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_UInt32 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_UInt32_new(),
                    &UA_TYPES[UA_TYPES_UINT32]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_UInt32 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Int64 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Int64_new(),
                    &UA_TYPES[UA_TYPES_INT64]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Int64 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_UInt64 *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_UInt64_new(),
                    &UA_TYPES[UA_TYPES_INT64]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_UInt64 *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Float *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Float_new(),
                    &UA_TYPES[UA_TYPES_FLOAT]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Float *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Double *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Double_new(),
                    &UA_TYPES[UA_TYPES_DOUBLE]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Double *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_STRING]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_String *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_String_new(),
                    &UA_TYPES[UA_TYPES_STRING]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_String *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_DATETIME]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_DateTime *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_DateTime_new(),
                    &UA_TYPES[UA_TYPES_DATETIME]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_DateTime *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_GUID]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_Guid *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_Guid_new(),
                    &UA_TYPES[UA_TYPES_GUID]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_Guid *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_ByteString *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_ByteString_new(),
                    &UA_TYPES[UA_TYPES_BYTESTRING]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_ByteString *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_XMLELEMENT]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_XmlElement *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_XmlElement_new(),
                    &UA_TYPES[UA_TYPES_XMLELEMENT]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_XmlElement *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_NODEID]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_NodeId *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_NodeId_new(),
                    &UA_TYPES[UA_TYPES_NODEID]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_NodeId *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_EXPANDEDNODEID]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_ExpandedNodeId *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_ExpandedNodeId_new(),
                    &UA_TYPES[UA_TYPES_EXPANDEDNODEID]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_ExpandedNodeId *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_STATUSCODE]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_StatusCode *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_StatusCode_new(),
                    &UA_TYPES[UA_TYPES_STATUSCODE]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_StatusCode *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_QUALIFIEDNAME]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_QualifiedName *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_QualifiedName_new(),
                    &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_QualifiedName *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_LocalizedText *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_LocalizedText_new(),
                    &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_LocalizedText *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_ExtensionObject *) variant->data)[i], type,
                    dataValue);
        else {
            UA_Variant_setScalar(variant, UA_ExtensionObject_new(),
                    &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_ExtensionObject *) variant->data, type, dataValue);
        }
    } else if (type == &UA_TYPES[UA_TYPES_DATAVALUE]) {
        if (dataIndex != -1)
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    &((UA_DataValue *) variant->data)[i], type, dataValue);
        else {
            UA_Variant_setScalar(variant, UA_DataValue_new(),
                    &UA_TYPES[UA_TYPES_DATAVALUE]);
            sCode = zkUA_jsonDecode_UA_Variant_setData(
                    (UA_DataValue *) variant->data, type, dataValue);
        }
    }

    return sCode;
}

UA_StatusCode zkUA_jsonDecode_UA_Variant(json_t *value, UA_Variant *variant) {

    int sInt;

    /* First let's check if this is an empty UA_Variant */
    if (json_typeof(value) == JSON_STRING) {
        if (strcmp(json_string_value(value), "empty") == 0) {
            variant->type = NULL;
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant: Variant is empty (doesn't contain a scalar/array)\n");
            return UA_STATUSCODE_GOOD;
        }
    }

    const char *key;
    json_t *jvalue;
    /* Flags to ensure that the type, storageType, arrayLength, and arrayDimensionsSize objects are extracted */
    int hasType = 0, hasSType = 0, hasALength = 0, hasADSize = 0;
    size_t aDimS = 0;
    /* Iterate over all objects looking for the type, storageType, arrayLength, and arrayDimensionsSize objects */json_object_foreach(
            value, key, jvalue)
    {
        if (strcmp(key, "type") == 0) {
            hasType = 1;
            int typeInt;
            json_unpack(jvalue, "i", &typeInt);
            if (typeInt == 999) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_Variant: UA_Type set to 999 - Unknown or unsupported data type during encoding\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
            variant->type = &UA_TYPES[typeInt];
            if (variant->type == NULL) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_Variant: could not find the node type\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            } else
                fprintf(stderr, "zkUA_jsonDecode_UA_Variant: Node is type %s\n",
                        variant->type->typeName);
        } else if (strcmp(key, "storageType") == 0) {
            hasSType = 1;
            const char *sType = json_string_value(jvalue);
            if (sType == NULL) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_Variant: Error: Unable to extract UA_Variant storage type\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
            if (strcmp(sType, "UA_VARIANT_DATA") == 0) {
                variant->storageType = UA_VARIANT_DATA;
            } else if (strcmp(sType, "UA_VARIANT_DATA_NODELETE") == 0) {
                variant->storageType = UA_VARIANT_DATA_NODELETE;
            } else {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_Variant: Error: Unknown UA_Variant storage type\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
        } else if (strcmp(key, "arrayLength") == 0) {
            hasALength = 1;
            sInt = json_unpack(jvalue, "i", &variant->arrayLength);
            if (sInt != 0) {
                fprintf(stderr,
                        "zkUA_jsonDecode_UA_Variant: Unable to unpack arrayLength\n");
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
        } else if (strcmp(key, "arrayDimensionsSize") == 0) {
            hasADSize = 1;
            /* how many dimensions */
            sInt = json_unpack(jvalue, "i", &aDimS);
        }
    }
    /* We should always be able to extract the type, storageType and arrayLength */
    if (!(hasType && hasSType && hasALength)) {
        fprintf(stderr,
                "zkUA_jsonDecode_UA_Variant: Could not extract the type/storageType/arrayLength\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (variant->arrayLength != 0)
        if (!hasADSize) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant: Could not extract the arrayDimensionsSize\n");
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
    json_t *dataValue;
    if (variant->arrayLength == 0) { /* scalar */
        dataValue = json_object_get(value, "data");
        /* decode the scalar value */
        UA_StatusCode sCode = zkUA_jsonDecode_UA_Variant_callSetDataByType(
                variant, (UA_DataType *) variant->type, dataValue, -1);
        /* Return, there's no need to initialize arrays etc. */
        if (sCode != UA_STATUSCODE_GOOD) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant: Unable to decode for scalar \n");
        }
        if (!UA_Variant_isScalar(variant))
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant: Error! Variant is not set properly as a scalar!\n");
        variant->arrayLength = 0;
        return sCode;
    }

    /* initialize an array to hold the array values */
    void *arrayHolder = UA_Array_new(variant->arrayLength, variant->type);
    UA_Variant_setArray(variant, arrayHolder, variant->arrayLength,
            variant->type);

    /* extract the array of values */
    for (size_t i = 0; i < (variant->arrayLength); i++) {
        char *dataIndex = (char *) calloc(65535, sizeof(char));
        snprintf(dataIndex, 65535, "data[%lu]", i);
        json_t *dataValue = json_object_get(value, dataIndex);
        if (zkUA_jsonDecode_UA_Variant_callSetDataByType(variant,
                (UA_DataType *) variant->type, dataValue,
                i) != UA_STATUSCODE_GOOD) {
            fprintf(stderr,
                    "zkUA_jsonDecode_UA_Variant: zkUA_jsonDecode_UA_Variant_callSetDataByType failed - arrayLength %lu - Data Index %lu\n",
                    variant->arrayLength, i);
            free(dataIndex);
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        free(dataIndex);
    }

    UA_Variant_setArray(variant, arrayHolder, variant->arrayLength,
            variant->type);

    /* initialize an array to hold the array dimensions */
    variant->arrayDimensionsSize = aDimS;
    if (variant->arrayDimensionsSize == 0)
        variant->arrayDimensions = NULL;
    else {
        variant->arrayDimensions = UA_Array_new(variant->arrayDimensionsSize,
                &UA_TYPES[UA_TYPES_INT32]);
        fprintf(stderr,
                "zkUA_jsonDecode_UA_Variant: Created an arrayDimensions array of size %lu\n",
                variant->arrayDimensionsSize);
    }
    /* extract the array dimensions */
    size_t aDimI;
    char *aDim = (char *) calloc(65535, sizeof(char));
    for (size_t i = 0; i < (variant->arrayDimensionsSize); i++) {
        snprintf(aDim, 65535, "arrayDimensions[%lu]", i);
        json_t *aDimObject = json_object_get(value, aDim);
        sInt = json_unpack(aDimObject, "i", &aDimI);
        if (sInt != 0)
            fprintf(stderr, "zkUA_jsonDecode_UA_Variant: Could not unpack %s\n",
                    aDim);
        else {
            variant->arrayDimensions[i] = (int) aDimI;
        }
        memset(aDim, 0, 65535);
    }
    free(aDim);

    return UA_STATUSCODE_GOOD;
}

/**
 * zkUA_jsonDecode_zkNodeToUa_commonAttributes:
 * Decodes the following common attributes from a JSON object and places it in a given attributes struct:
 * DisplayName, Description, WriteMask, UserWriteMask
 * They are common to all node classes
 */
UA_StatusCode zkUA_jsonDecode_zkNodeToUa_commonAttributes(json_t *attributes,
        void *opcuaAttributes) {
    UA_StatusCode sCode;
    /* cast as UA_ObjectAttributes */
    UA_ObjectAttributes *objectAttributes =
            (UA_ObjectAttributes *) opcuaAttributes;

    /* Extract the common node attributes */
    json_t *displayName = json_object_get(attributes, "displayName");
    json_t *description = json_object_get(attributes, "description");
    if (!json_is_object(displayName) || !json_is_object(description)) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributes: Could not retrieve the displayName/description JSON objects\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    sCode = zkUA_jsonDecode_UA_LocalizedText(displayName,
            &objectAttributes->displayName);
    if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributes: Could not decode the displayName\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    sCode = zkUA_jsonDecode_UA_LocalizedText(description,
            &objectAttributes->description);
    if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributes: Could not decode the description\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    return UA_STATUSCODE_GOOD;
}

/**
 * zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType
 * Decodes the attributes that are common to Variable and VariableType node classes
 */
UA_StatusCode zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType(
        json_t *attributes, void *opcuaAttributes) {

    UA_StatusCode sCode;

    /* cast as UA_VariableAttributes */
    UA_VariableAttributes *variableAttributes =
            (UA_VariableAttributes *) opcuaAttributes;

    /* Decode the attributes common to all node classes */
    sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
            variableAttributes);
    if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType: Could not decode commonAttributes\n");
        return sCode;
    }

    /* Decode the attributes common to Variable and VariableType Attributes */
    json_t *value = json_object_get(attributes, "value");
    if (!json_is_object(value)) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType: Unable to extract value object\n");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    sCode = zkUA_jsonDecode_UA_Variant(value, &variableAttributes->value);
    if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType: Could not decode the variant value\n");
        return sCode;
    }

    json_t *dataType = json_object_get(attributes, "dataType");
    sCode = zkUA_jsonDecode_UA_NodeId(dataType, &variableAttributes->dataType);
    if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType: Could not decode the dataType\n");
        return sCode;
    }

    json_t *valueRank = json_object_get(attributes, "valueRank");
    json_unpack(valueRank, "i", &variableAttributes->valueRank);

    json_t *arrayDimensionsSize = json_object_get(attributes,
            "arrayDimensionsSize");
    json_unpack(arrayDimensionsSize, "i",
            &variableAttributes->arrayDimensionsSize);
    return sCode;

}

UA_StatusCode zkUA_jsonDecode_zkNodeToUa_Variable(json_t *attributes,
        UA_VariableAttributes *variableAttributes) {

    /* Decode the common attributes */
    UA_StatusCode sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType(
            attributes, variableAttributes);

    /* Decode the nodeClass specific attributes */
    size_t tmpAL, tmpUAL;
    json_t *accessLevel = json_object_get(attributes, "accessLevel");
    json_t *userAccessLevel = json_object_get(attributes, "userAccessLevel");
    json_unpack(accessLevel, "i", &tmpAL);
    json_unpack(userAccessLevel, "u", &tmpUAL);
    variableAttributes->accessLevel = (UA_Byte) tmpAL;
    variableAttributes->userAccessLevel = (UA_Byte) tmpUAL;

    json_t *minimumSamplingInterval = json_object_get(attributes,
            "minimumSamplingInterval");
    json_t *historizing = json_object_get(attributes, "historizing");
    json_unpack(minimumSamplingInterval, "f",
            &variableAttributes->minimumSamplingInterval);
    json_unpack(historizing, "b", &variableAttributes->historizing);
    return sCode;
}

UA_StatusCode zkUA_jsonDecode_writeVariableAttributes(UA_Server *server,
        UA_NodeId *uaNodeId, UA_VariableAttributes *vAtt) {

    UA_StatusCode sCode;

    sCode = UA_Server_writeDisplayName(server, *uaNodeId, vAtt->displayName);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeDisplayName\n");
        return sCode;
    }
    UA_Server_writeDescription(server, *uaNodeId, vAtt->description);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeDescription\n");
        return sCode;
    }
    UA_Server_writeWriteMask(server, *uaNodeId, vAtt->writeMask);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeWriteMask\n");
        return sCode;
    }
    UA_Server_writeValue(server, *uaNodeId, vAtt->value);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeValue\n");
        return sCode;
    }
    UA_Server_writeDataType(server, *uaNodeId, vAtt->dataType);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeDataType\n");
        return sCode;
    }
    UA_Server_writeValueRank(server, *uaNodeId, vAtt->valueRank);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeValueRank\n");
        return sCode;
    }
    UA_Server_writeArrayDimensions(server, *uaNodeId, vAtt->value);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeArrayDimensions\n");
        return sCode;
    }
    UA_Server_writeAccessLevel(server, *uaNodeId, vAtt->accessLevel);
    if (sCode != UA_STATUSCODE_GOOD) {
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeAccessLevel\n");
        return sCode;
    }
    UA_Server_writeMinimumSamplingInterval(server, *uaNodeId,
            vAtt->minimumSamplingInterval);
    if (sCode != UA_STATUSCODE_GOOD)
        fprintf(stderr,
                "zkUA_jsonDecode_writeVariableAttributes: Couldn't writeMinimumSamplingInterval\n");

    return sCode;
}

UA_StatusCode zkUA_jsonDecode_writeNamespaceArrayAttributes(UA_Server *server,
        UA_NodeId *uaNodeId, json_t *attributes) {
    //The below is for adding new namespaces to the local server via zk replication
    UA_StatusCode sCode;
    UA_VariableAttributes variableAttributes;
    UA_VariableAttributes_init(&variableAttributes);
    sCode = zkUA_jsonDecode_zkNodeToUa_Variable(attributes,
            &variableAttributes);

    if (sCode != UA_STATUSCODE_GOOD)
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: Error decoding node ns=%d;i=%d\n",
                uaNodeId->namespaceIndex, uaNodeId->identifier.numeric);
    int newNsIndex;
    char *nsString = calloc(65535, sizeof(char));
    for (size_t i = 2; i < (variableAttributes.value.arrayLength); i++) {
        snprintf(nsString, 65535, "%.*s",
                (int) ((UA_String *) variableAttributes.value.data)[i].length,
                ((UA_String *) variableAttributes.value.data)[i].data);
        newNsIndex = UA_Server_addNamespace(uaServer, nsString);
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: Added a new namespace %s Index %d\n",
                nsString, newNsIndex);
        memset(nsString, 0, 65535);
    }
    free(nsString);
    UA_Variant_deleteMembers(&variableAttributes.value);
    UA_VariableAttributes_deleteMembers(&variableAttributes);
    return sCode;
}

void zkUA_jsonDecode_rewriteNodeAttributes(UA_Server *server,
        UA_NodeId *uaNodeId, int nC, json_t *attributes) {

    fprintf(stderr,
            "zkUA_jsonDecode_rewriteNodeAttributes: Function called for node ns=%d;i=%d\n",
            uaNodeId->namespaceIndex, uaNodeId->identifier.numeric);

    UA_StatusCode sCode;

    switch (nC) {
    case UA_NODECLASS_OBJECT: {
        break;
    }
    case UA_NODECLASS_VIEW: {
        break;
    }
    case UA_NODECLASS_VARIABLE: {
        UA_VariableAttributes variableAttributes;
        UA_VariableAttributes_init(&variableAttributes);
        /* Decode the node attributes */
        sCode = zkUA_jsonDecode_zkNodeToUa_Variable(attributes,
                &variableAttributes);
        fprintf(stderr,
                "zkUA_jsonDecode_rewriteNodeAttributes: Rewriting variable attributes of node ns=%d;i=%d\n",
                uaNodeId->namespaceIndex, uaNodeId->identifier.numeric);
        if (sCode != UA_STATUSCODE_GOOD)
            fprintf(stderr,
                    "zkUA_jsonDecode_rewriteNodeAttributes: Error decoding attributes of node ns=%d;i=%d\n",
                    uaNodeId->namespaceIndex, uaNodeId->identifier.numeric);
        /*        if (!UA_Variant_isScalar(&variableAttributes.value))
         fprintf(stderr,
         "zkUA_jsonDecode_rewriteNodeAttributes: Node ns=%d;i=%d is not a scalar!\n",
         uaNodeId->namespaceIndex, uaNodeId->identifier.numeric);
         else {
         fprintf(stderr,
         "zkUA_jsonDecode_rewriteNodeAttributes: Node ns=%d;i=%d is a scalar - arrayLength is %lu value is %d\n",
         uaNodeId->namespaceIndex, uaNodeId->identifier.numeric,
         variableAttributes.value.arrayLength,
         *(UA_Int32 *) variableAttributes.value.data);
         sCode = UA_Server_writeValue(uaServer, (const UA_NodeId) *uaNodeId,
         variableAttributes.value);
         }*/

        // testing: Manually changing the value to a different redundancy type
        //                    UA_Int32 newVal = true;
        //                    UA_Variant_setScalar(&variableAttributes.value, &newVal, &UA_TYPES[UA_TYPES_INT32]);
        /* writeVariableAttributes causes it to replicate again to zK also BadTypeMismatch and BadWriteNotSupported errors. */
        zkUA_jsonDecode_writeVariableAttributes(uaServer, uaNodeId,
                &variableAttributes);
        UA_Variant_deleteMembers(&variableAttributes.value);
        UA_VariableAttributes_deleteMembers(&variableAttributes);
        break;
    }
    case UA_NODECLASS_VARIABLETYPE: {
        break;
    }
    case UA_NODECLASS_REFERENCETYPE: {
        break;
    }
    case UA_NODECLASS_OBJECTTYPE: {
        break;
    }
    case UA_NODECLASS_DATATYPE: {
        break;
    }
    case UA_NODECLASS_METHOD: {
        break;
    }
    default: {

    }
        return;
    }
}

void zkUA_jsonDecode_zkNodeToUa(int rc, const char *value, int value_len,
        const struct Stat *stat, const void *server) {
    /* TODO: For atomicity: insert mzxid if successful, rollback mzxid if not -
     Note: atomicity of updating the znode/UA node is handled in the intercepted functions  */
    /* Set the UA_Server for use in all functions */
    uaServer = (UA_Server *) server;

    if (!value) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: Error! Value not returned - rc = %d\n",
                rc);
        return;
    }

    json_error_t error;
    UA_StatusCode sCode;

//    fprintf(stderr, "Data completion %s rc = %d\n",(char*)data,rc);
//    fprintf(stderr, "value %.*s\n", value_len, value);

    /* load the retrieved JSON into an object */
    json_t *jsonRoot = json_loads(value, JSON_DISABLE_EOF_CHECK, &error);
    if (!jsonRoot) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: Unable to load root object from retrieved JSON document - error: on line %d: %s\n",
                error.line, error.text);
        return;
        //        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    /* Get the NodeInfo object to retrieve the nodeId, nodeClass,
     parentNodeId, parentReferenceNodeId, browsePath, and restPath */
    json_t *nodeInfo = json_object_get(jsonRoot, "NodeInfo");
    if (!json_is_object(nodeInfo)) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: nodeInfo retrieval did not return an object\n");
        json_decref(jsonRoot);
        return;
        //        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    } else {
        //        print_json(nodeInfo);
    }

    json_t *nodeId = json_object_get(nodeInfo, "NodeId");
    UA_NodeId uaNodeId;
    if (!json_is_object(nodeId)) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: NodeId retrieval did not return a number\n");
        json_decref(jsonRoot);
        return;
        //        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    } else {
        /* TODO: Handle other identifier types, e.g.,
         * plug memleak when identifier.string is in use */
        sCode = zkUA_jsonDecode_UA_NodeId(nodeId, &uaNodeId);
    }

    /* Decode the parentNodeId and parentReferenceNodeId*/
    /* Note: There is no need for you to decode the references, only the parent node */
    json_t *parentNodeId = json_object_get(nodeInfo, "parentNodeId");
    json_t *parentReferenceNodeId = json_object_get(nodeInfo,
            "parentReferenceNodeId");
    UA_NodeId dParentNodeId, dParentReferenceNodeId;
    sCode = zkUA_jsonDecode_UA_NodeId(parentNodeId, &dParentNodeId);
    sCode = zkUA_jsonDecode_UA_NodeId(parentReferenceNodeId,
            &dParentReferenceNodeId);

    /* Get the nodeClass so that we can know how to extract the attributes */
    json_t *nodeClass = json_object_get(nodeInfo, "NodeClass");
    if (!json_is_number(nodeClass)) {
        fprintf(stderr,
                "zkUA_jsonDecode_zkNodeToUa: NodeClass retrieval did not return a number\n");
        json_decref(jsonRoot);
        UA_NodeId_deleteMembers(&uaNodeId);
        return;
        //        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    } else {
        int nC;
        json_unpack(nodeClass, "i", &nC);

        /* Get the Attributes object to retrieve.. the node's attributes! */
        json_t *attributes = json_object_get(jsonRoot, "Attributes");
        if (!json_is_object(attributes)) {
            fprintf(stderr,
                    "zkUA_jsonDecode_zkNodeToUa: NodeClass retrieval did not return a number\n");
            json_decref(jsonRoot);
            UA_NodeId_deleteMembers(&uaNodeId);
            return;
            //            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        } else {
            //            fprintf(stderr, "zkUA_jsonDecode_zkNodeToUa: Retrieved attributes object: %s\n", json_dumps(attributes,JSON_INDENT(1)));

            /* ``The only Nodes that can differ between Servers in a Redundant Server Set
             are the Nodes that are in the local Server namespace like the Server diagnostic Nodes."
             - Section 6.4.2.2 - Pg 113 of 189 of Part 4 of R1.03 specs. */
            /* If this is a namespace (NS) Index 0 node */
            if (uaNodeId.namespaceIndex == 0) {
                /* Exception for the Server sub-tree is the namespaceIndex node */
                if (uaNodeId.identifier.numeric
                        == UA_NS0ID_SERVER_NAMESPACEARRAY) {
                    /* Rewrite the known namespace Indexes for the local server */
                    zkUA_jsonDecode_writeNamespaceArrayAttributes(uaServer,
                            &uaNodeId, attributes);
                    json_decref(jsonRoot);
                    UA_NodeId_deleteMembers(&uaNodeId);
                    return;
                }
                /* Otherwise let's check if we disqualify the node because it is part of the
                 Server node sub-tree */
                zkUA_checkNs0 *searchedForNode = calloc(1,
                        sizeof(zkUA_checkNs0));
                searchedForNode->searchedForNode = &uaNodeId;
                searchedForNode->result = false;
                UA_Server_forEachChildNodeCall(uaServer,
                        UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER),
                        zkUA_jsonDecode_checkChildOfNS0ServerNode,
                        (void *) searchedForNode);
                if (searchedForNode->result == true) {
                    /* If this is a server object node */
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Node ns=%d;i=%d is part of the Server Node's subtree\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                    json_decref(jsonRoot);
                    UA_NodeId_deleteMembers(&uaNodeId);
                    free(searchedForNode);
                    return;
                }
                /* TODO: Otherwise we re-write the attributes of the existing NS0 nodes
                 * or create a new node if it doesn't already exist */
                fprintf(stderr,
                        "zkUA_jsonDecode_zkNodeToUa: Node ns=%d;i=%d is not a child of the Server Node\n",
                        uaNodeId.namespaceIndex, uaNodeId.identifier.numeric);
                /* disabled - incomplete & non-functional */
                /*zkUA_jsonDecode_rewriteNodeAttributes(uaServer, &uaNodeId, nC,
                 attributes);*/
                free(searchedForNode);
                json_decref(jsonRoot);
                UA_NodeId_deleteMembers(&uaNodeId);
                return;

            }
            /* Otherwise this is a non-NS0 node and we add the node as normal */
            switch (nC) {
            case UA_NODECLASS_OBJECT: {
                /* Extract the object's attributes */
                UA_ObjectAttributes objectAttributes;
                UA_ObjectAttributes_init(&objectAttributes);

                /*
                 char *s = json_dumps(attributes, JSON_INDENT(1));
                 fprintf(stderr, "zkUA_jsonDecode_zkNodeToUa: Attributes object should still be valid: %s\n", s);
                 free(s);
                 */
                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &objectAttributes);

                /* Decode the nodeClass specific attributes */
                size_t tmpEventNotifier;
                json_t *eventNotifier = json_object_get(attributes,
                        "eventNotifier");
                json_unpack(eventNotifier, "i", &tmpEventNotifier);

                objectAttributes.eventNotifier = (UA_Byte) tmpEventNotifier;

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) objectAttributes.displayName.text.length,
                        objectAttributes.displayName.text.data);
                const UA_QualifiedName qName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addObjectNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, qName,
                        UA_NODEID_NULL, objectAttributes, NULL, NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added Object node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    //                    sCode = UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete Object node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted Object node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addObjectNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, qName,
                            UA_NODEID_NULL, objectAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add Object node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added Object node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add Object node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }
                UA_ObjectAttributes_deleteMembers(&objectAttributes);
                //                UA_LocalizedText_deleteMembers(&objectAttributes.displayName);
                //                UA_LocalizedText_deleteMembers(&objectAttributes.description);
                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_VIEW: {
                UA_ViewAttributes viewAttributes;
                UA_ViewAttributes_init(&viewAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &viewAttributes);

                /* Decode the nodeClass specific attributes */
                size_t tmpEventNotifier;
                json_t *eventNotifier = json_object_get(attributes,
                        "eventNotifier");
                json_unpack(eventNotifier, "i", &tmpEventNotifier);
                viewAttributes.eventNotifier = (UA_Byte) tmpEventNotifier;

                json_t *containsNoLoops = json_object_get(attributes,
                        "containsNoLoops");
                json_unpack(containsNoLoops, "b",
                        &viewAttributes.containsNoLoops);

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) viewAttributes.displayName.text.length,
                        viewAttributes.displayName.text.data);
                const UA_QualifiedName qName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addViewNode(uaServer, uaNodeId, dParentNodeId,
                        dParentReferenceNodeId, qName, viewAttributes, NULL,
                        NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added View node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete View node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted View node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addViewNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, qName,
                            viewAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add View node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added View node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);

                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add View node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }

                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_VARIABLE: {
                UA_VariableAttributes variableAttributes;
                UA_VariableAttributes_init(&variableAttributes);
                sCode = zkUA_jsonDecode_zkNodeToUa_Variable(attributes,
                        &variableAttributes);
                if (sCode != UA_STATUSCODE_GOOD)
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not decode JSON objects for Variable node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) variableAttributes.displayName.text.length,
                        variableAttributes.displayName.text.data);
                UA_QualifiedName varName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);
                //                size_t aDimSize = variableAttributes.value.arrayDimensionsSize;
                //                fprintf(stderr, "zkUA_jsonDecode_zkNodeToUa: aDimSize %lu arrayDimensionsSize %lu node ns = %d nId = %d\n", aDimSize, variableAttributes.value.arrayDimensionsSize, uaNodeId.namespaceIndex, uaNodeId.identifier.numeric);
                sCode = UA_Server_addVariableNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, varName,
                        UA_NODEID_NULL, variableAttributes, NULL, NULL);
                if (UA_Variant_isEmpty(&variableAttributes.value))
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Variant is empty \n");
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added Variable node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete Variable node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted Variable node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addVariableNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, varName,
                            UA_NODEID_NULL, variableAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add Variable node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added Variable node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add Variable node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }
                //                fprintf(stderr,"zkUA_jsonDecode_zkNodeToUa: Freeing Variable node ns = %d nId = %d\n", uaNodeId.namespaceIndex, uaNodeId.identifier.numeric);
                UA_Variant_deleteMembers(&variableAttributes.value);
                UA_VariableAttributes_deleteMembers(&variableAttributes);
                //                fprintf(stderr,"zkUA_jsonDecode_zkNodeToUa: Free'd Variable node ns = %d nId = %d\n", uaNodeId.namespaceIndex, uaNodeId.identifier.numeric);
                UA_LocalizedText_deleteMembers(&variableAttributes.displayName);
                UA_LocalizedText_deleteMembers(&variableAttributes.description);
                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_VARIABLETYPE: {
                UA_VariableTypeAttributes variableTypeAttributes;
                UA_VariableTypeAttributes_init(&variableTypeAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributesVarVarType(
                        attributes, &variableTypeAttributes);

                /* Decode the nodeClass specific attributes */
                json_t *isAbstract = json_object_get(attributes, "isAbstract");
                json_unpack(isAbstract, "b",
                        &variableTypeAttributes.isAbstract);

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) variableTypeAttributes.displayName.text.length,
                        variableTypeAttributes.displayName.text.data);
                UA_QualifiedName varName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addVariableTypeNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, varName,
                        UA_NODEID_NULL, variableTypeAttributes, NULL, NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added VariableType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete VariableType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted VariableType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addVariableTypeNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, varName,
                            UA_NODEID_NULL, variableTypeAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add VariableType node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add VariableType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }
                UA_VariableTypeAttributes_deleteMembers(
                        &variableTypeAttributes);
                UA_LocalizedText_deleteMembers(
                        &variableTypeAttributes.displayName);
                UA_LocalizedText_deleteMembers(
                        &variableTypeAttributes.description);
                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_REFERENCETYPE: {
                UA_ReferenceTypeAttributes referenceTypeAttributes;
                UA_ReferenceTypeAttributes_init(&referenceTypeAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &referenceTypeAttributes);

                /* Decode the nodeClass specific attributes */
                json_t *isAbstract = json_object_get(attributes, "isAbstract");
                json_unpack(isAbstract, "b",
                        &referenceTypeAttributes.isAbstract);

                json_t *symmetric = json_object_get(attributes, "symmetric");
                json_unpack(symmetric, "b", &referenceTypeAttributes.symmetric);

                json_t *inverseName = json_object_get(attributes,
                        "inverseName");
                sCode = zkUA_jsonDecode_UA_LocalizedText(inverseName,
                        &referenceTypeAttributes.inverseName);
                if (sCode == UA_STATUSCODE_BADUNEXPECTEDERROR) {
                    json_decref(jsonRoot);
                    UA_NodeId_deleteMembers(&uaNodeId);
                    UA_LocalizedText_deleteMembers(
                            &referenceTypeAttributes.inverseName);
                    return;
                }
                //                    return UA_STATUSCODE_BADUNEXPECTEDERROR;

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) referenceTypeAttributes.displayName.text.length,
                        referenceTypeAttributes.displayName.text.data);
                const UA_QualifiedName qName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addReferenceTypeNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, qName,
                        referenceTypeAttributes, NULL, NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added ReferenceType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete ReferenceType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added ReferenceType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addReferenceTypeNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, qName,
                            referenceTypeAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add ReferenceType node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added ReferenceType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add ReferenceType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }
                UA_ReferenceTypeAttributes_deleteMembers(
                        &referenceTypeAttributes);
                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_OBJECTTYPE: {
                UA_ObjectTypeAttributes objectTypeAttributes;
                UA_ObjectTypeAttributes_init(&objectTypeAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &objectTypeAttributes);

                /* Decode the nodeClass specific attributes */
                json_t *isAbstract = json_object_get(attributes, "isAbstract");
                json_unpack(isAbstract, "b", &objectTypeAttributes.isAbstract);

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) objectTypeAttributes.displayName.text.length,
                        objectTypeAttributes.displayName.text.data);
                const UA_QualifiedName qName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addObjectTypeNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, qName,
                        objectTypeAttributes, NULL, NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added ObjectType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete ObjectType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted ObjectType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addObjectTypeNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, qName,
                            objectTypeAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add ObjectType node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added ObjectType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add ObjectType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }

                UA_LocalizedText_deleteMembers(
                        &objectTypeAttributes.displayName);
                UA_LocalizedText_deleteMembers(
                        &objectTypeAttributes.description);
                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_DATATYPE: {
                UA_DataTypeAttributes dataTypeAttributes;
                UA_DataTypeAttributes_init(&dataTypeAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &dataTypeAttributes);

                /* Decode the nodeClass specific attributes */
                json_t *isAbstract = json_object_get(attributes, "isAbstract");
                json_unpack(isAbstract, "b", &dataTypeAttributes.isAbstract);

                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                snprintf(qNameBrowseName, 65535, "%.*s",
                        (int) dataTypeAttributes.displayName.text.length,
                        dataTypeAttributes.displayName.text.data);
                const UA_QualifiedName qName = UA_QUALIFIEDNAME(
                        uaNodeId.namespaceIndex, qNameBrowseName);

                sCode = UA_Server_addDataTypeNode(uaServer, uaNodeId,
                        dParentNodeId, dParentReferenceNodeId, qName,
                        dataTypeAttributes, NULL, NULL);
                if (sCode == UA_STATUSCODE_GOOD) {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Added DataType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                } else if (sCode == UA_STATUSCODE_BADNODEIDEXISTS) {
                    /* Delete the node that exists locally */
                    //                    sCode = zkUA_UA_Server_deleteNode(uaServer, uaNodeId, false /* delete references */);
                    sCode = zkUA_UA_Server_deleteNode_dontReplicate(uaServer,
                            uaNodeId, false /* delete references */);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not delete DataType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Deleted DataType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    /* Add the newly decoded node */
                    sCode = UA_Server_addDataTypeNode(uaServer, uaNodeId,
                            dParentNodeId, dParentReferenceNodeId, qName,
                            dataTypeAttributes, NULL, NULL);
                    if (sCode != UA_STATUSCODE_GOOD)
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Could not add DataType node ns = %d nId = %d after deletion of locally existing node\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                    else
                        fprintf(stderr,
                                "zkUA_jsonDecode_zkNodeToUa: Added DataType node ns = %d nId = %d\n",
                                uaNodeId.namespaceIndex,
                                uaNodeId.identifier.numeric);
                } else {
                    fprintf(stderr,
                            "zkUA_jsonDecode_zkNodeToUa: Could not add DataType node ns = %d nId = %d\n",
                            uaNodeId.namespaceIndex,
                            uaNodeId.identifier.numeric);
                }

                free(qNameBrowseName);
                break;
            }
            case UA_NODECLASS_METHOD: {
                UA_MethodAttributes methodAttributes;
                UA_MethodAttributes_init(&methodAttributes);

                /* Decode the common attributes */
                sCode = zkUA_jsonDecode_zkNodeToUa_commonAttributes(attributes,
                        &methodAttributes);

                /* Decode the nodeClass specific attributes */
                json_t *exec = json_object_get(attributes, "executable");
                //                json_t *userExec = json_object_get(attributes, "userExecutable");
                json_unpack(exec, "b", &methodAttributes.executable);
                //                json_unpack(userExec, "b", &methodAttributes.userExecutable);

                /* todo: include support for method */
                fprintf(stderr,
                        "zkUA_jsonDecode_zkNodeToUa: Replicating Method nodes currently unsupported\n");
                /*                char * qNameBrowseName = (char *) calloc(65535, sizeof(char));
                 snprintf(qNameBrowseName, 65535, "%.*s", (int) methodAttributes.displayName.text.length, methodAttributes.displayName.text.data);
                 const UA_QualifiedName qName = UA_QUALIFIEDNAME(uaNodeId.namespaceIndex, qNameBrowseName);
                 *//* temporarily disabled addition of UA_Method until I write up a way for transferrability of methods
                 or set all method nodes to call the hello world function till then for memory profiling purposes*/
                /*                UA_Server_addMethodNode(server, uaNodeId, dParentNodeId, dParentReferenceNodeId,
                 qName, UA_NODEID_NULL, methodAttributes, NULL, NULL);
                 free(qNameBrowseName);
                 */
                UA_MethodAttributes_deleteMembers(&methodAttributes);
                json_decref(jsonRoot);
                UA_NodeId_deleteMembers(&uaNodeId);
                return;
                //                return UA_STATUSCODE_BADMETHODINVALID;
                break;
            }
            default:
                fprintf(stderr, "UA_NodeClass is unspecified or unknown!\n");
                sCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
        }
    }
    json_decref(jsonRoot);
    UA_NodeId_deleteMembers(&uaNodeId);
    return;
    /*
     if(sCode == UA_STATUSCODE_BADUNEXPECTEDERROR)
     return UA_STATUSCODE_BADUNEXPECTEDERROR;
     else
     return UA_STATUSCODE_GOOD;
     */
}
