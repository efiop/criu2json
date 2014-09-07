#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "protobuf2json.h"
#include "log.h"

#ifndef json_boolean_value
#define json_boolean_value(json) ((json) && json_typeof(json) == JSON_TRUE)
#endif

static int pb_field_to_json(const ProtobufCFieldDescriptor *fd, const void *pb_field, json_t **js_field)
{
	pr_info("Start converting field %s to json\n", fd->name);

	switch (fd->type) {
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
		pr_info("Type: int32\n");

		*js_field = json_integer(*(int32_t *)pb_field);
		break;
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_FIXED32:
		pr_info("Type: uint32\n");

		*js_field = json_integer(*(uint32_t *)pb_field);
		break;
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
		pr_info("Type: int64\n");

		*js_field = json_integer(*(int64_t *)pb_field);
		break;
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_FIXED64:
		pr_info("Type: uint64\n");

		*js_field = json_integer(*(uint64_t *)pb_field);
		break;
	case PROTOBUF_C_TYPE_FLOAT:
		pr_info("Type: float\n");

		*js_field = json_real(*(float *)pb_field);
		break;
	case PROTOBUF_C_TYPE_DOUBLE:
		pr_info("Type: double\n");

		*js_field = json_real(*(double *)pb_field);
		break;
	case PROTOBUF_C_TYPE_BOOL:
		pr_info("Type: bool\n");

		*js_field = json_boolean(*(protobuf_c_boolean *)pb_field);
		break;
	case PROTOBUF_C_TYPE_ENUM:
		{
		pr_info("Type: enum\n");

		const ProtobufCEnumValue *pb_enum_val = protobuf_c_enum_descriptor_get_value(fd->descriptor, *(int *)pb_field);
		if (!pb_enum_val) {
			pr_err("Unknown enum value\n");
			return -1;
		}

		*js_field = json_string((char *)pb_enum_val->name);

		break;
		}
	case PROTOBUF_C_TYPE_STRING:
		pr_info("Type: string\n");

		*js_field = json_string(*(char **)pb_field);
		break;
	case PROTOBUF_C_TYPE_BYTES:
		{
		pr_info("Type: bytes\n");

		//FIXME does it work??? look at cr-show to figure out
		const ProtobufCBinaryData *pb_bin = (const ProtobufCBinaryData *)pb_field;
		*js_field = json_string((const char *)pb_bin->data);//, pb_bin->len); FIXME check if value doesnt change
		break;
		}
	case PROTOBUF_C_TYPE_MESSAGE:
		{
		pr_info("Type: message\n");

		const ProtobufCMessage **pb = (const ProtobufCMessage **)pb_field;
		if (protobuf_to_json((*pb)->descriptor, (void *)(*pb), js_field)) {
			pr_err("Failed to convert field to json\n");
			return -1;
		}
		break;
		}
	default:
		pr_err("Unknown field type");
		return -1;
	}

	pr_info("Done converting field %s\n", fd->name);

	return 0;
}

static size_t get_size_of_pb_type(ProtobufCType type)
{
	switch (type) {
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_FIXED32:
	case PROTOBUF_C_TYPE_FLOAT:
	case PROTOBUF_C_TYPE_ENUM:
		return 4;
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_FIXED64:
	case PROTOBUF_C_TYPE_DOUBLE:
		return 8;
	case PROTOBUF_C_TYPE_STRING:
		return sizeof(char *);
	case PROTOBUF_C_TYPE_BOOL:
		return sizeof(protobuf_c_boolean);
	case PROTOBUF_C_TYPE_BYTES:
		return sizeof(ProtobufCBinaryData);
	case PROTOBUF_C_TYPE_MESSAGE:
		return sizeof(ProtobufCMessage *);
	default:
		return 0;
	}
}

int protobuf_to_json(const ProtobufCMessageDescriptor *pb_desc, const void *pb, json_t **js)
{
	int i, ret;
	json_t *js_field = NULL;

	*js = json_object();
	if (!*js) {
		pr_err("Can't allocate json object\n");
		goto err;
	}

	for (i = 0; i < pb_desc->n_fields; i++) {
		const ProtobufCFieldDescriptor *fd = pb_desc->fields + i;
		const void *pb_field = pb + fd->offset;
		const void *pb_quant = pb + fd->quantifier_offset;
		int j;

		pr_info("Start processing field %s\n", fd->name);

		switch(fd->label) {
		case PROTOBUF_C_LABEL_REQUIRED:
			{
			pr_info("Field %s label: required\n", fd->name);

			ret = pb_field_to_json(fd, pb_field, &js_field);
			break;
			}
		case PROTOBUF_C_LABEL_OPTIONAL:
			{
			bool has = false;

			pr_info("Field %s label: optional\n", fd->name);

			if (fd->type == PROTOBUF_C_TYPE_MESSAGE ||
			    fd->type == PROTOBUF_C_TYPE_STRING) {
				if (*(const void * const *)pb_field)
					has = true;
			} else {
				if (*(const protobuf_c_boolean *)pb_quant)
					has = true;
			}

			if (has)
				ret = pb_field_to_json(fd, pb_field, &js_field);
			else
				continue;

			break;
			}
		case PROTOBUF_C_LABEL_REPEATED:
			{
			size_t n_values, value_size;
		       
			pr_info("Field %s label: repeated\n", fd->name);

			n_values = *(const size_t *)pb_quant;
			if (n_values == 0)
				continue;

			js_field = json_array();
			if (!js_field) {
				pr_err("Can't allocate json array for field %s\n", fd->name);
				goto err;
			}

			value_size = get_size_of_pb_type(fd->type);
			if (value_size == 0) {
				pr_err("Unknown type of field %s\n", fd->name);
				goto err;
			}

			for (j = 0; j < n_values; j++) {
				const void *value = *((const void * const *)pb_field) + j * value_size;
				json_t *js_value = NULL;

				ret = pb_field_to_json(fd, value, &js_value);
				if (ret)
					break;

				ret = json_array_append_new(js_field, js_value);
				if (ret) {
					pr_err("Can't append to json array\n");
					break;
				}
			}

			break;
			}
		default:
			pr_err("Unknown label of field %s\n", fd->name);
			goto err;
		}

		if (ret)
			goto err;

		pr_info("Adding %s to json\n", fd->name);
		ret = json_object_set_new(*js, fd->name, js_field);
		if (ret) {
			pr_err("Can't add %s field to json message", fd->name);
			goto err;
		}

		pr_info("Done processing field %s\n", fd->name);
	}

	return 0;

err:
	if (*js)
		json_decref(*js);

	return -1;
}

static int js_field_to_pb(const ProtobufCFieldDescriptor *fd, json_t *js_field, void **pb_field)
{
	pr_info("Start converting field %s to pb\n", fd->name);

	switch (fd->type) {
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
		{
		int32_t val;

		pr_info("Type: int32\n");

		if (!json_is_integer(js_field)) {
			pr_err("json object is not an integer\n");
			return -1;
		}

		val = (int32_t)json_integer_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_FIXED32:
		{
		uint32_t val;

		pr_info("Type: uint32\n");

		if (!json_is_integer(js_field)) {
			pr_err("json object is not an integer\n");
			return -1;
		}
		
		val = (uint32_t)json_integer_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
		{
		int64_t val;

		pr_info("Type: int64\n");

		if (!json_is_integer(js_field)) {
			pr_err("json object is not an integer\n");
			return -1;
		}

		val = (int64_t)json_integer_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_FIXED64:
		{
		uint64_t val;

		pr_info("Type: uint64\n");

		if (!json_is_integer(js_field)) {
			pr_err("json object is not an integer\n");
			return -1;
		}

		val = (uint64_t)json_integer_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_FLOAT:
		{
		float val;

		pr_info("Type: float\n");

		if (!json_is_real(js_field)) {
			pr_err("json object is not a real\n");
			return -1;
		}

		val = (float)json_real_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_DOUBLE:
		{
		double val;

		pr_info("Type: double\n");

		if (!json_is_real(js_field)) {
			pr_err("json object is not a real\n");
			return -1;
		}

		val = (double)json_real_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_BOOL:
		{
		protobuf_c_boolean val;

		pr_info("Type: bool\n");

		if (!json_is_boolean(js_field)) {
			pr_err("json object is not a boolean\n");
			return -1;
		}

		val = (protobuf_c_boolean)json_boolean_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_ENUM:
		{
		const char *val_name;
		int32_t val;
		const ProtobufCEnumValue *val_enum;

		pr_info("Type: enum\n");

		if (!json_is_string(js_field)) {
			pr_err("json object is not a string(enum)\n");
			return -1;
		}

		val_name = json_string_value(js_field);
		val_enum = protobuf_c_enum_descriptor_get_value_by_name(fd->descriptor, val_name);
		if (!val_enum) {
			pr_err("Unknown enum value\n");
			return -1;
		}

		val = (int32_t)val_enum->value;
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_STRING:
		{
		const char *val;

		pr_info("Type: string\n");

		if (!json_is_string(js_field)) {
			pr_err("json object is not a string\n");
			return -1;
		}

		val = json_string_value(js_field);
		memcpy(pb_field, &val, sizeof(val));
		break;
		}
	case PROTOBUF_C_TYPE_BYTES:
		{
		ProtobufCBinaryData *bin;

		pr_info("Type: bytes\n");

		bin = malloc(sizeof(*bin));
		if (!bin) {
			pr_err("Can't allocate mem for bin\n");
			return -1;
		}

		if (!json_is_string(js_field)) {
			pr_err("json object is not a string(bytes)\n");
			return -1;
		}

		bin->data = (uint8_t *)json_string_value(js_field);
		bin->len = sizeof(bin->data);

		memcpy(pb_field, &bin, sizeof(bin));
		break;
		}
	case PROTOBUF_C_TYPE_MESSAGE:
		{
		ProtobufCMessage *pb;

		pr_info("Type: message\n");

		if(json_to_protobuf(fd->descriptor, js_field, (void **)&pb))
			return -1;

		memcpy(pb_field, &pb, sizeof(pb));
		break;
		}
	default:
		pr_err("Unknown field type");
		return -1;	
	}

	pr_info("Done converting field %s to pb\n", fd->name);

	return 0;
}

int json_to_protobuf(const ProtobufCMessageDescriptor *pb_desc, json_t *js, void **pb)
{
	int ret = 0;

	const char *js_key;
	json_t *js_val;

	void *pb_field;
	void *pb_quant;

	if (!json_is_object(js)) {
		pr_err("Not a json object\n");
		goto err;
	}

	*pb = malloc(pb_desc->sizeof_message);
	if (!*pb) {
		pr_err("Can't allocate memory for pb\n");
		goto err;
	}

	protobuf_c_message_init(pb_desc, *(ProtobufCMessage **)pb);

	json_object_foreach(js, js_key, js_val) {
		const ProtobufCFieldDescriptor *fd = protobuf_c_message_descriptor_get_field_by_name(pb_desc, js_key);

		pr_info("Start processing field %s\n", js_key);

		if (!fd) {
			pr_err("Can't get field descriptor\n");
			goto err;
		}

		pb_field = *pb + fd->offset;
		pb_quant = *pb + fd->quantifier_offset;

		switch (fd->label) {
		case PROTOBUF_C_LABEL_REQUIRED:

			pr_info("Field %s label: required\n", js_key);

			ret = js_field_to_pb(fd, js_val, pb_field);
			break;
		case PROTOBUF_C_LABEL_OPTIONAL:
			{

			pr_info("Field %s label: optional\n", js_key);
				
			if (fd->type == PROTOBUF_C_TYPE_MESSAGE ||
			    fd->type == PROTOBUF_C_TYPE_STRING) {
				//FIXME delete?
			} else
				*(protobuf_c_boolean *)pb_quant = 1;

			ret = js_field_to_pb(fd, js_val, pb_field);
			break;
			}
		case PROTOBUF_C_LABEL_REPEATED:
			{
			size_t *n_values;
			size_t value_size;
			void *pb_array;
			size_t index;
			json_t *value;
			void *pb_array_val;

			pr_info("Field %s label: repeated\n", js_key);

			if (!json_is_array(js_val)) {
				pr_err("Not an array\n");
				goto err;
			}

			n_values = (size_t *)pb_quant;
			*n_values = json_array_size(js_val);
			if (!*n_values)
				break;

			value_size = get_size_of_pb_type(fd->type);
			if (value_size == 0) {
				pr_err("Unknown type of field %s\n", fd->name);
				goto err;
			}

			pb_array = malloc(*n_values * value_size);
			if (!pb_array) {
				pr_err("Can't alloc array for field %s\n", fd->name);
				goto err;
			}

			json_array_foreach(js_val, index, value) {
				pb_array_val = pb_array + index * value_size;

				ret = js_field_to_pb(fd, value, pb_array_val);
				if (ret)
					goto err;
			}

			memcpy(pb_field, &pb_array, sizeof(pb_array));

			break;
			}
		default:
			pr_err("Unknown label of field %s\n", fd->name);
			goto err;
		}

		if (ret)
			goto err;

		pr_info("Done processing field %s\n", js_key);
	}

	return 0;
err:
	return -1;
}
