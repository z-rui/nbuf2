#include "benchmark_generated.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <ctime>

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>

#include "common.h"

static void create_serialize(flatbuffers::FlatBufferBuilder &builder)
{
	static const std::vector<float> vec = { 3.141, 2.718, 1.618 };

	builder.Clear();
	std::vector<flatbuffers::Offset<Entry>> entries(MAX_ENTRY);
	for (size_t i = 0; i < MAX_ENTRY; i++) {
		auto coordinates = (i % 7 == 0) ?
			builder.CreateVector<float>(vec) : 0;
		auto msg = (i % 2 == 0) ?
			builder.CreateString("100 bottles on the wall") : 0;
		EntryBuilder b_(builder);
		if (i % 3 == 0)
			b_.add_magic(0xDEADBEEFull * i);
		b_.add_id(i);
		if (i % 5 == 0)
			b_.add_pi(3.14159265358979323846 + i);
		b_.add_coordinates(coordinates);
		b_.add_msg(msg);
		entries[i] = b_.Finish();
	}
	auto root = CreateRootDirect(builder, &entries);
	builder.Finish(root);
}

static void verify(const flatbuffers::FlatBufferBuilder &builder)
{
	flatbuffers::Verifier v(builder.GetBufferPointer(), builder.GetSize());
	bool ok = VerifyRootBuffer(v);
	if (!ok)
		abort();
}

static void deserialize_use(const Root *root)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i = 0;

	for (const Entry *entry : *root->entries()) {
		if (i % 3 == 0)
			assert(entry->magic() == 0xDEADBEEFull * i);
		assert(entry->id() == (int) i);
		if (i % 5 == 0)
			assert(entry->pi() == 3.14159265358979323846 + i);
		if (i % 7 == 0) {
			const auto *coordinates = entry->coordinates();
			for (size_t j = 0; j < 3; j++)
				assert((*coordinates)[j] == vec[j]);
		}
		if (i % 2 == 0) {
			assert(entry->msg()->size() == strlen("100 bottles on the wall"));
		}
		i++;
	}
}

static void print_text_format(const flatbuffers::Parser &parser,
	const flatbuffers::FlatBufferBuilder &builder, std::string *buf)
{
	buf->clear();
	GenerateText(parser, builder.GetBufferPointer(), buf);
}

static void parse_text_format(flatbuffers::Parser *parser,
	const std::string &buf)
{
	bool ok = parser->Parse(buf.c_str());
	assert(ok);
}

int main()
{
	flatbuffers::FlatBufferBuilder builder;
	BENCH(create_serialize(builder), 100000);
	BENCH(verify(builder), 100000);
	const Root *root = GetRoot(builder.GetBufferPointer());
	BENCH(deserialize_use(root), 100000);
	std::string buf, schemafile;
	flatbuffers::LoadFile("benchmark.fbs", /*binary=*/false, &schemafile);
	flatbuffers::Parser parser;
	parser.Parse(schemafile.c_str());
	BENCH(print_text_format(parser, builder, &buf), 2000);
	flatbuffers::SaveFile("benchmark_fb.json", buf.data(), buf.size(), /*binary=*/false);
	BENCH(parse_text_format(&parser, buf), 2000);
	return 0;
}
