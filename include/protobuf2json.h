#include <jansson.h>
#include <google/protobuf-c/protobuf-c.h>

extern int protobuf_to_json(const ProtobufCMessageDescriptor *pb_desc, const void *pb, json_t **js);
extern int json_to_protobuf(const ProtobufCMessageDescriptor *pb_desc, json_t *js, void **pb);
