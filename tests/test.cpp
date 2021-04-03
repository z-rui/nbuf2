#include "test.nb.hpp"
#include "libnbuf.h"
#include <iostream>

int main()
{
	nbuf::buffer buf;
	const char output[] = "test.bin";

	if (!::nbuf_load_file(&buf, output))
		return 1;
	auto log_file = LogFile::get(&buf);
	if (!log_file)
		return 1;
	for (auto entry : log_file.log_entry()) {
		auto timestamp = entry.timestamp();
		if (timestamp) {
			std::cout << timestamp.seconds() << '.'
				<< timestamp.nanoseconds() << " ";
		}
		std::cout << '(' << (int) entry.severity() << "): " <<
			entry.message() << std::endl;
	}
}
