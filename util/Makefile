CC=gcc
all: verify_golden

verify_golden: verify_golden.c
	$(CC) verify_golden.c -lz -o verify_golden

clean:
	rm -rf verify_golden
