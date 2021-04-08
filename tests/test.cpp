#include "test.nb.hpp"
#include "libnbuf.h"

#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace {

static constexpr int TOTAL_LOGS = 100;
static constexpr const char OUTPUT[] = "test.bin";

std::string makemsg(int n)
{
	std::stringstream ss;

	ss << n << " bottles of beer on the wall, " << n << " bottles of beer\n" <<
		"Take one down and pass it around, " << n-1 << " bottles of beer on the wall\n";
	return ss.str();
}

static void write_msg()
{
	int i = 0;
	nbuf::buffer buf;

	nbuf_init_rw(&buf, 4096);
	auto log_file = LogFile::alloc(&buf);
	for (auto entry : log_file.alloc_log_entry(TOTAL_LOGS)) {
		using logging::LogSeverity;
		auto timestamp = entry.alloc_timestamp();
		struct timespec ts;

		clock_gettime(CLOCK_REALTIME, &ts);
		timestamp.set_seconds(ts.tv_sec);
		timestamp.set_nanoseconds(ts.tv_nsec);
		entry.set_message(makemsg(100-i));
		entry.set_severity(
			i > 98 ? LogSeverity::CRITICAL :
			i > 90 ? LogSeverity::WARNING :
			i > 60 ? LogSeverity::NOTICE :
			LogSeverity::INFO);
		i++;
		usleep(1000);
	}
	std::cerr << "dumping log to " << OUTPUT << std::endl;
	nbuf_save_file(&buf, OUTPUT);
	nbuf_clear(&buf);
}

static void read_msg()
{
	nbuf::buffer buf;

	fprintf(stderr, "loading log from %s\n", OUTPUT);
	nbuf_load_file(&buf, OUTPUT);
	auto logfile = LogFile::get(&buf);

	for (auto entry : logfile.log_entry()) {
		auto timestamp = entry.timestamp();
		std::cout << timestamp.seconds() << '.' <<
			timestamp.nanoseconds() << " (" <<
			int(entry.severity()) << "): " <<
			entry.message();
	}
	nbuf_unload_file(&buf);
}

}  // namespace

int main()
{
	write_msg();
	read_msg();
	return 0;
}
