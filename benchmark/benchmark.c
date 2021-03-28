#include "benchmark.nb.h"
#include "libnbuf.h"

#include <assert.h>

#include "common.h"

static void create_serialize(struct nbuf *buf)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i, j;
	Root root;
	Entry entry;

	buf->len = 0;
	alloc_Root(&root, buf);
	Root_alloc_entries(&entry, root, MAX_ENTRY);
	for (i = 0; i < MAX_ENTRY; i++) {
		if (i % 3 == 0)
			Entry_set_magic(entry, 0xDEADBEEFull * i);
		Entry_set_id(entry, i);
		if (i % 5 == 0)
			Entry_set_pi(entry, 3.14159265358979323846 + i);
		if (i % 7 == 0) {
			float *coord = (float *) Entry_alloc_coordinates(entry, 3);
			for (j = 0; j < 3; j++)
				nbuf_set_f32(&coord[j], vec[j]);
		}
		if (i % 2 == 0)
			Entry_set_msg(entry, "100 bottles on the wall", -1);
		nbuf_next(NBUF_OBJ(entry));
	}
}

static void deserialize_use(struct nbuf *buf)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	Root root;
	Entry entry;
	size_t n;
	size_t i, j, len;

	get_Root(&root, buf, 0);
	n = Root_entries(&entry, root, 0);
	assert(n == MAX_ENTRY);
	for (i = 0; i < n; i++) {
		if (i % 3 == 0)
			assert(Entry_magic(entry) == 0xDEADBEEFull * i);
		assert(Entry_id(entry) == (int) i);
		if (i % 5 == 0)
			assert(Entry_pi(entry) == 3.14159265358979323846 + i);
		if (i % 7 == 0) {
#if 1  // faster alternative
			const float *coord;
			struct nbuf_obj o;
			Entry_raw_coordinates(&o, entry);
			coord = (const float *) nbuf_obj_base(&o);
			for (j = 0; j < 3; j++)
				assert(nbuf_f32(&coord[j]) == vec[j]);
#else
			for (j = 0; j < 3; j++)
				assert(Entry_coordinates(entry, j) == vec[j]);
#endif
		}
		if (i % 2 == 0) {
			Entry_msg(entry, &len);
			assert(len == strlen("100 bottles on the wall"));
		}
		nbuf_next(NBUF_OBJ(entry));
	}
}

static void print_text_format(FILE *f, Root root)
{
	extern const nbuf_MsgDef refl_Root;
	struct nbuf_print_opt opt = { .f = f, .indent = 2 };
	nbuf_print(&opt, NBUF_OBJ(root), refl_Root);
}

static void parse_text_format(struct nbuf *in, struct nbuf *out)
{
	extern const nbuf_MsgDef refl_Root;
	struct nbuf_parse_opt opt = { .outbuf = out, .filename = "benchmark.nb.txt" };
	struct nbuf_obj o;

	out->len = 0;
	nbuf_parse(&opt, &o, in->base, in->len, refl_Root);
}

int main()
{
	struct nbuf buf;
	Root root;

	memset(&buf, 0, sizeof buf);
	BENCH(create_serialize(&buf), 100000);
	nbuf_save_file(&buf, "benchmark.nb.bin");
	BENCH(deserialize_use(&buf), 100000);
	get_Root(&root, &buf, 0);
	{
		FILE *f = fopen(NUL_FILE, "w");
		BENCH(print_text_format(f, root), 2000);
		fclose(f);
	}
	{
		FILE *f = fopen("benchmark.nb.txt", "wb");
		print_text_format(f, root);
		fclose(f);
	}
	{
		struct nbuf buf1;
		nbuf_clear(&buf);
		nbuf_load_file(&buf1, "benchmark.nb.txt");
		BENCH(parse_text_format(&buf1, &buf), 2000);
		nbuf_unload_file(&buf1);
	}
	// get_Root(&root, &buf, 0);
	// print_text_format(stdout, root);
	nbuf_clear(&buf);
	return 0;
}
