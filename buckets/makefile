all: hashgen gen_search search bsearch sort psort

hashgen: hashgen.c common.c
	gcc -O3 -o hashgen hashgen.c blake3.c blake3_dispatch.c blake3_portable.c     blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S     blake3_avx512_x86-64_unix.S

gen_search: gen_search.c common.c
	gcc -o gen_search gen_search.c

search: search.c common.c
	gcc -o search search.c

sort: sort.c common.c
	gcc -o sort sort.c

psort: psort.c common.c
	gcc -o psort psort.c -lpthread

bsearch: bsearch.c common.c
	gcc -o bsearch bsearch.c -lm

clean:
	rm -f hashgen gen_search search bsearch testsort sort psort