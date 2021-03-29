#include "potato.nb.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TTL 500
#define MAX_HOSTS 1000

static void add_trace(Potato potato, uint16_t hostid)
{
	size_t len;
	struct nbuf_obj o;

	len = Potato_raw_trace(&o, potato);
	if (len == 0) {
		if (!Potato_alloc_trace(potato, 1))
			return;
	} else {
		if (!nbuf_alloc(o.buf, sizeof hostid))
			return;
		nbuf_resize_arr(&o, len+1);
	}
	Potato_set_trace(potato, len, hostid);
}

static void print_trace(Potato potato)
{
	size_t i, n;

	n = Potato_trace_size(potato);

	printf("total hops: %u\n", (unsigned) n);
	printf("trace:");
	for (i = 0; i < n; i++) {
		printf(" %u", Potato_trace(potato, i));
	}
	putchar('\n');
}

int main() {
	struct nbuf_buf buf[1];
	Potato potato;

	srand(time(NULL));
	nbuf_init_rw(buf, 4096);
	alloc_Potato(&potato, buf);
	Potato_set_ttl(potato, rand() % MAX_TTL);

	printf("expected hops: %u\n", (unsigned) Potato_ttl(potato));
	while (Potato_ttl(potato) > 0) {
		add_trace(potato, rand() % MAX_HOSTS);
		Potato_set_ttl(potato, Potato_ttl(potato) - 1);
	}
	print_trace(potato);
}
