#include "benchmark.capnp.h"

#include <cassert>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <fstream>
#include <streambuf>

#include <capnp/serialize.h>
#include <capnp/serialize-text.h>

#include "common.h"

static void create_serialize(capnp::MessageBuilder *builder)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };

	auto root = builder->initRoot<Root>();
	auto entries = root.initEntries(MAX_ENTRY);
	for (size_t i = 0; i < MAX_ENTRY; i++) {
		auto entry = entries[i];
		if (i % 3 == 0)
			entry.setMagic(0xDEADBEEFull * i);
		entry.setId(i);
		if (i % 5 == 0)
			entry.setPi(3.14159265358979323846 + i);
		if (i % 7 == 0) {
			auto coordinates = entry.initCoordinates(3);
			for (size_t j = 0; j < 3; j++)
				coordinates.set(j, vec[j]);
		}
		if (i % 2 == 0)
			entry.setMsg("100 bottles on the wall");
	}
}

static void deserialize_use(capnp::MessageBuilder &builder)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i = 0;

	auto root = builder.getRoot<Root>();
	for (auto entry : root.getEntries()) {
		if (i % 3 == 0)
			assert(entry.getMagic() == 0xDEADBEEFull * i);
		assert(entry.getId() == (int) i);
		if (i % 5 == 0)
			assert(entry.getPi() == 3.14159265358979323846 + i);
		if (i % 7 == 0) {
			auto coordinates = entry.getCoordinates();
			for (size_t j = 0; j < 3; j++)
				assert(coordinates[j] == vec[j]);
		}
		if (i % 2 == 0)
			assert(entry.getMsg().size() == strlen("100 bottles on the wall"));
		i++;
	}
	assert(i == MAX_ENTRY);
}

template <typename T>
static void print_text_format(T &&root, kj::String *s)
{
	capnp::TextCodec textcodec;
	textcodec.setPrettyPrint(true);
	*s = textcodec.encode(root);
}

static void parse_text_format(capnp::MallocMessageBuilder *builder, const kj::StringPtr &buf)
{
	capnp::TextCodec textcodec;
	textcodec.decode<Root>(buf, builder->getOrphanage());
}

int main()
{
	BENCH({capnp::MallocMessageBuilder builder; create_serialize(&builder);}, 100000);
	capnp::MallocMessageBuilder builder;
	create_serialize(&builder);
	auto root = builder.getRoot<Root>();
	{
		FILE *fp = fopen("benchmark.cp.bin", "w");
		writeMessageToFd(fileno(fp), builder);
		BENCH(deserialize_use(builder), 100000);
		fclose(fp);
	}
	{
		kj::String buf;
		BENCH(print_text_format(root, &buf), 2000);
		std::fstream f("benchmark.capnp.txt", std::ios::out | std::ios::binary);
		f << buf.cStr();
		BENCH({capnp::MallocMessageBuilder builder; parse_text_format(&builder, buf);}, 2000);
	}

	return 0;
}
