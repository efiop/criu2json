#include <jansson.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>

#include "log.h"
#include "protobuf2json.h"
#include "criu2json.h"

bool verbose;

static int read_pb(int fd, void **pb, struct protobuf_info *info)
{
	int size, ret = -1;
	void *buf = NULL;

	ret = read(fd, &size, sizeof(size));
	if (ret == 0)
		return 0;
	else if (ret != sizeof(size)) {
		pr_err("Can't read size of protobuf message\n");
		goto out;
	}

	buf = malloc(size);
	if (!buf) {
		pr_err("Can't allocate mem for pb message\n");
		goto out;
	}

	if (read(fd, buf, size) != size) {
		pr_err("Can't read pb message\n");
		goto out;
	}

	*pb = info->unpack(NULL, size, buf);
	if (*pb == NULL) {
		pr_err("Can't unpack pb message\n");
		goto out;
	}

	ret = 1;
out:
	if (buf)
		free(buf);
	return ret;
}

static int img_to_json(char in[], char out[])
{
	uint32_t magic;
	int fd_in, ret = -1, i;
	char *json_string = NULL;
	json_t *js = NULL;
	struct criu_image_info *info = NULL;

	fd_in = open(in, O_RDONLY);
	if (fd_in < 0) {
		pr_perror("Can't open input file");
		goto out;
	}

	if (read(fd_in, &magic, sizeof(magic)) != sizeof(magic)) {
		pr_perror("Can't read magic from input file");
		goto out;
	}

	for (i = 0; img_infos[i].magic; i++) {
		if (img_infos[i].magic == magic) {
			info = &img_infos[i];
			break;
		}
	}

	if (!info) {
		pr_err("Unknown magic");
		goto out;
	}

	js = json_object();
	if (json_object_set_new(js, "magic", json_integer(info->magic))) {
		pr_err("Can't write magic to json\n");
		goto out;
	}

	for (i = 0; ; i++) {
		void *obj;
		struct protobuf_info *pb_info = NULL;
		json_t *js_entry = NULL;
		char name[16];

		if (i == 0)
			pb_info = &info->header_info;
		else if (i > 0 && info->is_array)
			pb_info = &info->extra_info;
		else
			break;

		ret = read_pb(fd_in, &obj, pb_info);
		if (ret < 0)
			goto out;
		else if (ret == 0)
			break;

		if (protobuf_to_json(pb_info->desc, obj, &js_entry)) {
			pr_err("Can't convert to json");
			pb_info->free(obj, NULL);
			goto out;
		}

		pb_info->free(obj, NULL);

		sprintf(name, "%d", i);

		if (json_object_set_new(js, name, js_entry)) {
			pr_err("Can't write entry to json");
			goto out;
		}
	}

	ret = json_dump_file(js, out, JSON_INDENT(4));
	if (ret) {
		pr_err("Can't dump json object");
		goto out;
	}

	ret = 0;
out:
	if (fd_in >= 0)
		close(fd_in);
	return ret;
}

static int json_to_img(char in[], char out[])
{
	uint32_t magic;
	int fd_out = -1, i = 0, ret = -1;
	json_t *js = NULL;
	json_error_t jerror;
	char file_img[PATH_MAX];
	json_t *js_magic;
	char js_key[16];
	json_t *js_value;
	void *pb = NULL, *buf = NULL;
	int pb_size;
	struct criu_image_info *info = NULL;
	
	js = json_load_file(in, 0, &jerror);
	if (!js) {
		pr_err("json parsing error at line %d col %d pos %d: %s\n",
			jerror.line, jerror.column, jerror.position, jerror.text);
		goto out;
	}

	js_magic = json_object_get(js, "magic");
	if (!js_magic) {
		pr_err("No magic key found\n");
		goto out;
	}

	magic = (uint32_t)json_integer_value(js_magic);

	for (i = 0; img_infos[i].magic; i++) {
		if (img_infos[i].magic == magic) {
			info = &img_infos[i];
			break;
		}
	}

	if (!info) {
		pr_err("Unknown magic\n");
		goto out;
	}

	fd_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd_out <= 0) {
		pr_perror("Can't open output file");
		goto out;
	}

	ret = write(fd_out, &magic, sizeof(magic));
	if (ret != sizeof(magic)) {
		pr_perror("Can't write magic to img");
		goto out;
	}

	for (i = 0; ; i++) {
		int packed;
		struct protobuf_info *pb_info = NULL;

		ret = -1;

		if (i == 0)
			pb_info = &info->header_info;
		else if (i > 0 && info->is_array)
			pb_info = &info->extra_info;
		else
			break;

		snprintf(js_key, sizeof(js_key), "%d", i);

		js_value = json_object_get(js, js_key);
		if (!js_value)
			break;

		ret = json_to_protobuf(pb_info->desc, js_value, &pb);
		if (ret) {
			pr_err("Can't convert json object #%d to protobuf\n", i);
			goto out_for;
		}

		pb_size = pb_info->getpksize(pb);

		ret = write(fd_out, &pb_size, sizeof(pb_size));
		if (ret != sizeof(pb_size)) {
			pr_perror("Can't write #%d object size %d\n", i, pb_size);
			goto out_for;
		}

		buf = malloc(pb_size);
		if (!buf) {
			pr_err("Can't allocate buffer for packed pb object\n");
			goto out_for;
		}

		packed = pb_info->pack(pb, buf);
		if (packed != pb_size) {
			pr_err("Failed to pack pb object\n");
			goto out_for;
		}

		ret = write(fd_out, buf, pb_size);
		if (ret != pb_size) {
			pr_perror("Can't write #%d object\n", i);
			goto out_for;
		}

		ret = 0;
out_for:
		if (buf) {
			free(buf);
			buf = NULL;
		}
		pb_info->free(pb, NULL);
		pb=NULL;
		if (ret)
			goto out;
	}

	ret = 0;
out:
	if (fd_out >= 0)
		close(fd_out);
	return ret;
}

int main(int argc, char *argv[])
{
	if (argc != 4 && argc != 5)
		goto usage;
	if (argc == 5)
		if (!strcmp(argv[4], "-v") || !strcmp(argv[4], "--verbose"))
			verbose = true;

	if (!strcmp(argv[1], "to-json"))
		return img_to_json(argv[2], argv[3]);
	else if (!strcmp(argv[1], "to-img"))
		return json_to_img(argv[2], argv[3]);

usage:
	printf(
	"Usage: criu2json OPTION SOURCE DEST [verbose]\n"
	"Convert criu image to\\from json.\n"
	"\n"
	"Options:\n"
	"to-json           convert SOURCE criu image to json format and store it in DEST file\n"
	"to-img            convert SOURCE json file to criu image and store it in DEST file\n"
	"-v --verbose      be verbose\n"
	"\n"
	"Report criu2json bugs to kupruser@gmail.com\n");
	return 1;
}
