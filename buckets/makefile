all: hashgen gen_search search bsearch sort psort

hashgen: hashgen.c common.c
	gcc -o hashgen hashgen.c

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