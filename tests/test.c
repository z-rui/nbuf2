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

char *makemsg(int n)
{
	char *s;

	if (asprintf(&s, "%d bottles of beer on the wall, %d bottles of beer\n"
		"Take one down and pass it around, %d bottles of beer on the wall\n",
		n, n, n-1) == -1)
			return NULL;
	return s;
}

int main()
{
	int i;
	struct nbuf_buf buf[1];
	LogFile logfile;
	logging_LogEntry entries;
	const char output[] = "test.bin";

#if 0
	if (nbuf_load_file(buf, output)) {
		struct nbuf_print_opt opt = {
			.f = stdout,
			.indent = 2,
		};
		get_LogFile(&logfile, buf, 0);
		nbuf_print(&opt, NBUF_OBJ(logfile), refl_LogFile);
		nbuf_unload_file(buf);
		unlink(output);
	}
#endif
	{
		struct nbuf_print_opt opt = {
			.f = stdout,
			.indent = 2,
		};
		/* print self schema */
		nbuf_print(&opt, NBUF_OBJ(refl_LogFile), nbuf_refl_MsgDef);
	}
	nbuf_init_rw(buf, 4096);
	alloc_LogFile(&logfile, buf);
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
	fprintf(stderr, "dumping log to %s\n", output);
	nbuf_save_file(buf, output);
	nbuf_clear(buf);
	return 0;
}
