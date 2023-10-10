# proof-of-space-experimentation

Experiments - https://docs.google.com/spreadsheets/d/1rC3Hw5eIk5FdgjtKiqmmEBE9X27FKG7A/edit#gid=297450551

# FILES 

1. bseach.c - Contains code to perform binary search in the binary file
2. common.c - Conatins global variables and common functions
3. gen_search.c - Generates random list of hashed to search
4. psort.c - Conatins code to sort buckets in parallel
5. search.c - Containes code to peroform normal search
6. sort.c - Contains code to perform normal sort

# INSTRUCTIONS

All global configurable paramertes contained in "common.c"

You can build all code using "make" command

Steps conduct experiment

1. Configure the params of experiment in "common.c"

2. Build the code using "make" command

```
make
```

3. Generate hashes

```
./hashgen
```

A file called "plot.memo" should be created.

4. Perform sorting

To perform sequential sorting run,
```
./sort
```

To perform parallel sorting run,
```
./psort
```

5. Generate hashes to search

Run
```
./gen_search
```

6. Perform search

To perform binary search run,
```
./bsearch
```

To perform normal searching run,
```
./search
```
