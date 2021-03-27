set -o errexit
set -o xtrace

CFLAGS="-Wall -g -O2 -I../include"

for action in generate use; do
	gcc ${CFLAGS} -fprofile-${action} -o benchmark benchmark.c benchmark.nb.c ../lib/libnbuf.a
	./benchmark
done
