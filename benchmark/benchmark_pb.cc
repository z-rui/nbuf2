#include "benchmark.pb.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <ctime>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#define BENCH(expr, n) do { \
	clock_t start = std::clock(), stop; \
	for (int i = 0; i < n; i++) expr; \
	stop = std::clock(); \
	std::fprintf(stderr, "%s: %.fns/op\n", #expr, \
		(double) (stop - start) / (n) / CLOCKS_PER_SEC * 1e9); \
} while (0)

#define MAX_ENTRY 100

static void create(Root *root)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };

	root->Clear();
	for (size_t i = 0; i < MAX_ENTRY; i++) {
		Entry *entry = root->add_entries();
		if (i % 3 == 0)
			entry->set_magic(0xDEADBEEFull * i);
		entry->set_id(i);
		if (i % 5 == 0)
			entry->set_pi(3.14159265358979323846 + i);
		if (i % 7 == 0)
			for (size_t j = 0; j < 3; j++)
				entry->add_coordinates(vec[j]);
		if (i % 2 == 0)
			entry->set_msg("100 bottles on the wall");
	}
}

static std::string serialize(const Root &root)
{
	std::string s;
	root.SerializeToString(&s);
	return s;
}

static void deserialize(Root *root, const std::string s)
{
	root->ParseFromString(s);
}

static void use(const Root &root)
{
	static const float vec[3] = { 3.141, 2.718, 1.618 };
	size_t i = 0;

	for (const auto &entry : root.entries()) {
		if (i % 3 == 0)
			assert(entry.magic() == 0xDEADBEEFull * i);
		assert(entry.id() == (int) i);
		if (i % 5 == 0)
			assert(entry.pi() == 3.14159265358979323846 + i);
		if (i % 7 == 0)
			for (size_t j = 0; j < 3; j++)
				assert(entry.coordinates(j) == vec[j]);
		if (i % 2 == 0)
			assert(entry.msg().size() == strlen("100 bottles on the wall"));
		i++;
	}
	assert(i == MAX_ENTRY);
}

static void print_text_format(google::protobuf::io::ZeroCopyOutputStream *s,
	const Root &root)
{
	google::protobuf::TextFormat::Print(root, s);
}

static void parse_text_format(const std::string &s,
	Root *root)
{
	google::protobuf::TextFormat::ParseFromString(s, root);
}


int main()
{
	Root root;
	std::string buf;
	BENCH(create(&root), 100000);
	{
		std::fstream f("benchmark.pb.bin", std::ios::out | std::ios::binary);
		google::protobuf::io::OstreamOutputStream s(&f);
		root.SerializeToZeroCopyStream(&s);
	}
	BENCH(buf = serialize(root), 100000);
	BENCH(deserialize(&root, buf), 100000);
	BENCH(use(root), 100000);
	{
		std::fstream f("/dev/null", std::ios::out | std::ios::binary);
		google::protobuf::io::OstreamOutputStream s(&f);
		BENCH(print_text_format(&s, root), 2000);
	}
	{
		std::fstream f("benchmark.pb.txt", std::ios::out | std::ios::binary);
		google::protobuf::io::OstreamOutputStream s(&f);
		print_text_format(&s, root);
	}
	{
		std::fstream f("benchmark.pb.txt", std::ios::in | std::ios::binary);
		std::string s((std::istreambuf_iterator<char>(f)),
		                 std::istreambuf_iterator<char>());
		BENCH(parse_text_format(s, &root), 2000);
	}
	// std::cout << root.DebugString() << '\n';
	return 0;
}
