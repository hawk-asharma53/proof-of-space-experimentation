all: hashgen phashgen gen_search search bsearch sort psort

hashgen: hashgen3.c common.c
	# gcc -O3 -o hashgen hashgen3.c blake3.c blake3_dispatch.c blake3_portable.c blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S
	gcc -shared -O3 -o hashgen hashgen3.c -DBLAKE3_USE_NEON=1 blake3.c blake3_dispatch.c blake3_portable.c blake3_neon.c

phashgen: hashgen4.c write_queue.c circular_queue.c common.c
	gcc -O3 -o phashgen hashgen4.c blake3.c blake3_dispatch.c blake3_portable.c  blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S -lpthread

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

hashgen_p: hashgen.c common.c
	gcc -pg -O3 -o hashgen_p hashgen.c blake3.c blake3_dispatch.c blake3_portable.c blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S

clean:
	rm -f hashgen gen_search search bsearch testsort sort psort