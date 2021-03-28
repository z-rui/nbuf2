#include <time.h>

#define BENCH(expr, n) do { \
	clock_t start = clock(), stop; \
	for (int i = 0; i < n; i++) expr; \
	stop = clock(); \
	fprintf(stderr, "%s: %.fns/op\n", #expr, \
		(double) (stop - start) / (n) / CLOCKS_PER_SEC * 1e9); \
} while (0)
#ifdef _WIN32
# define NUL_FILE "nul"
#else
# define NUL_FILE "/dev/null"
#endif

#define MAX_ENTRY 100

