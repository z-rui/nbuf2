#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include "test.nb.h"
#include "libnbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#define TOTAL_LOGS 100
#define OUTPUT "test.bin"

static char *makemsg(int n)
{
	char *s;

	if (asprintf(&s, "%d bottles of beer on the wall, %d bottles of beer\n"
		"Take one down and pass it around, %d bottles of beer on the wall\n",
		n, n, n-1) == -1)
			return NULL;
	return s;
}

static void write_msg(void)
{
	int i;
	struct nbuf_buf buf;
	LogFile logfile;
	logging_LogEntry entries;

	nbuf_init_ex(&buf, 4096);
	alloc_LogFile(&logfile, &buf);
	LogFile_alloc_log_entry(&entries, logfile, TOTAL_LOGS);

	for (i = 0; i < TOTAL_LOGS; i++) {
		logging_LogEntry entry;
		datetime_Timestamp timestamp;
		struct timespec ts;
		char *msg;

		LogFile_log_entry(&entry, logfile, i);
		logging_LogEntry_alloc_timestamp(&timestamp, entry);
		clock_gettime(CLOCK_REALTIME, &ts);
		datetime_Timestamp_set_seconds(timestamp, ts.tv_sec);
		datetime_Timestamp_set_nanoseconds(timestamp, ts.tv_nsec);
		msg = makemsg(100-i);
		logging_LogEntry_set_message(entry, msg, -1);
		free(msg);
		logging_LogEntry_set_severity(entry,
			i > 98 ? logging_LogSeverity_CRITICAL :
			i > 90 ? logging_LogSeverity_WARNING :
			i > 60 ? logging_LogSeverity_NOTICE :
			logging_LogSeverity_INFO);
		usleep(1000);
	}
	fprintf(stderr, "dumping log to %s, size = %zu\n", OUTPUT, buf.len);
	nbuf_save_file(&buf, OUTPUT);
	nbuf_clear(&buf);
}

static void read_msg(void)
{
	int i;
	struct nbuf_buf buf;
	LogFile logfile;
	logging_LogEntry entry;
	size_t n;

	nbuf_load_file(&buf, OUTPUT);
	fprintf(stderr, "loading log from %s, size = %zu\n", OUTPUT, buf.len);
	get_LogFile(&logfile, &buf, 0);
	n = LogFile_log_entry(&entry, logfile, 0);

	for (i = 0; i < n; i++) {
		datetime_Timestamp timestamp;
		uint32_t sec, nsec;
		const char *msg;
		logging_LogSeverity severity;

		logging_LogEntry_timestamp(&timestamp, entry);
		sec = datetime_Timestamp_seconds(timestamp);
		nsec = datetime_Timestamp_nanoseconds(timestamp);
		msg = logging_LogEntry_message(entry, NULL);
		severity = logging_LogEntry_severity(entry);
		printf("%d.%d (%s): %s", sec, nsec, logging_LogSeverity_to_string(severity), msg);
		nbuf_next(NBUF_OBJ(entry));
	}
	nbuf_clear(&buf);
}

int main()
{
	write_msg();
	read_msg();
	return 0;
}
