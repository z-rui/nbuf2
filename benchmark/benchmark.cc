#include "benchmark.nb.hpp"

#include <assert.h>

#include "common.h"

static void create_serialize(nbuf::buffer *buf)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i = 0;

	buf->len = 0;
	auto root = Root::alloc(buf);
	auto entries = root.alloc_entries(MAX_ENTRY);
	for (auto entry : entries) {
		if (i % 3 == 0)
			entry.set_magic(0xDEADBEEFull * i);
		entry.set_id(i);
		if (i % 5 == 0)
			entry.set_pi(3.14159265358979323846 + i);
		if (i % 7 == 0) {
			auto coord = entry.alloc_coordinates(3);
			for (size_t j = 0; j < 3; j++)
				coord[j] = vec[j];
		}
		if (i % 2 == 0)
			entry.set_msg("100 bottles on the wall");
		i++;
	}
}

static void deserialize_use(nbuf::buffer *buf)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i = 0;

	auto root = Root::get(buf);
	for (auto entry : root.entries()) {
		if (i % 3 == 0)
			assert(entry.magic() == 0xDEADBEEFull * i);
		assert(entry.id() == (int) i);
		if (i % 5 == 0)
			assert(entry.pi() == 3.14159265358979323846 + i);
		if (i % 7 == 0) {
			auto coord = entry.coordinates();
			for (size_t j = 0; j < 3; j++)
				assert(coord[j] == vec[j]);
		}
		if (i % 2 == 0)
			assert(entry.msg().size() == strlen("100 bottles on the wall"));
		i++;
	}
	assert(i == MAX_ENTRY);
}

int main()
{
	static char mem[8192];
	nbuf::buffer buf;

	nbuf_init_rw(&buf, mem, sizeof mem);
	BENCH(create_serialize(&buf), 100000);
	BENCH(deserialize_use(&buf), 100000);
	nbuf_clear(&buf);
	return 0;
}
