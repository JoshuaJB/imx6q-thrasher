/**
 * Copyright 2020 Joshua Bakita
 * Created September 19 2019
 * Description: This program is designed to stress the i.MX6 Quad memory bus
 * and controller as much as possible by purposely generating cache misses.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>

// L1 is 8-way with 64 lines/way
#define LINE_SIZE 64 // 8 64-bit words per line in L1, L2, and L3; 64 bytes
#define L3_SIZE 16*16384*64 // 16 ways, 16384 lines/way, 64 bytes/line

// Each piece of data can go in one of 16 ways
// Which bits do what for the L3?
// [ ...  |  way  |  line  |  byte  ]
//         23   20 19     6 5      0

// Get the current time in miliseconds
uint64_t get_ms() {
	struct timeval tv = {0};
	if (gettimeofday(&tv, NULL) < 0) {
		perror("Unable to get current time. Terminating...");
		exit(3);
	}
	return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

int main(int argc, char** argv) {
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        fprintf(stderr, "Usage: %s [number of iterations]\n", argv[0]);
        fprintf(stdout, "Program will iterate forever if the number of iterations is not specified.");
        return 1;
    }

    unsigned long iterations = 0;
    if (argc >= 2) {
        char* status = {'\0'};
        iterations = strtol(argv[1], &status, 10);
        if (iterations == LONG_MIN || iterations == LONG_MAX || status[0] != '\0') {
            perror("Invalid iteration count");
            return 1;
        }
    } else {
        fprintf(stdout, "Infinitely generating memory bus traffic...\n");
    }

    // Allocate a buffer meaningfully larger than the L2 such that when
    // iterating through, subsequent access to the same address will miss.
    // We make it generously larger as the L2 does not use true LRU.
    uint8_t* buffer = malloc(L3_SIZE * 4);

    if (!buffer) {
        perror("Unable to allocate buffer. Terminating...");
        return 2;
    }

    uint64_t start = get_ms();
    for (unsigned long i = 0; i < iterations || argc == 1; i++) {
        for (unsigned long i = 0; i < L3_SIZE * 4; i += LINE_SIZE) {
            buffer[i]++;
        }
    }
    uint64_t end = get_ms();

    // The below math relies on the L3 being at least 256KB
    double total_kbytes = (L3_SIZE/1024)*4*iterations;
    if (total_kbytes/(1<<20) >= 1)
        fprintf(stdout, "Completed generating %.1fGiB of memory requests", total_kbytes/(1<<20));
    else
        fprintf(stdout, "Completed generating %.1fMiB of memory requests", total_kbytes/(1<<10));
    fprintf(stdout, " in %.2f seconds.\n", (end - start) / 1000.0);

    free(buffer);
    return 0;
}
