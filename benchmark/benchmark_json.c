#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <json.h>

#include "common.h"

static struct json_object *create()
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	struct json_object *root, *entries;
	size_t i, j;

	root = json_object_new_object();
	entries = json_object_new_array();
	json_object_object_add(root, "entries", entries);
	for (i = 0; i < MAX_ENTRY; i++) {
		struct json_object *entry;

		entry = json_object_new_object();
		json_object_array_add(entries, entry);
		if (i % 3 == 0)
			json_object_object_add(entry, "magic",
				json_object_new_int64(0xDEADBEEFull * i));
		json_object_object_add(entry, "id",
			json_object_new_int(i));
		if (i % 5 == 0)
			json_object_object_add(entry, "pi",
				json_object_new_double(3.14159265358979323846 + i));
		if (i % 7 == 0) {
			struct json_object *coord = json_object_new_array();

			json_object_object_add(entry, "coordinates", coord);
			for (j = 0; j < 3; j++)
				json_object_array_add(coord,
					json_object_new_double(vec[j]));
		}
		if (i % 2 == 0)
			json_object_object_add(entry, "msg",
				json_object_new_string("100 bottles on the wall"));
	}
	return root;
}

static void use(struct json_object *root)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	struct json_object *entries;
	size_t n;
	size_t i, j;

	entries = json_object_object_get(root, "entries");
	n = json_object_array_length(entries);
	assert(n == MAX_ENTRY);
	for (i = 0; i < n; i++) {
		struct json_object *entry = json_object_array_get_idx(entries, i);
		if (i % 3 == 0)
			assert(json_object_get_int64(
				json_object_object_get(entry, "magic"))
				== 0xDEADBEEFull * i);
		assert(json_object_get_int(
			json_object_object_get(entry, "id"))
			== (int) i);
		if (i % 5 == 0)
			assert(json_object_get_double(
				json_object_object_get(entry, "pi"))
				== 3.14159265358979323846 + i);
		if (i % 7 == 0) {
			struct json_object *coord =
				json_object_object_get(entry, "coordinates");
			for (j = 0; j < 3; j++)
				assert(json_object_get_double(
					json_object_array_get_idx(coord, j))
					== vec[j]);
		}
		if (i % 2 == 0) {
			assert(json_object_get_string_len(
				json_object_object_get(entry, "msg"))
				== strlen("100 bottles on the wall"));
		}
	}
}

int main()
{
	struct json_object *root;
	const char *str;

	BENCH(json_object_put(create()), 10000);
	root = create();
	BENCH((void) json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN),
		10000);
	str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
	BENCH(json_object_put(json_tokener_parse(str)), 10000);
	BENCH(use(root), 10000);
	json_object_put(root);

	return 0;
}
