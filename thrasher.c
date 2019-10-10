/**
 * Copyright 2019 Joshua Bakita
 * Created September 19 2019
 * Description: This program is designed to stress the i.MX6 Quad memory bus
 * and controller as much as possible by purposely generating cache misses.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

// L1 is 4-way
#define LINE_SIZE 32 // 8 32-bit words per line in L1 & L2; 32 bytes
#define L2_SIZE 16*2048*32 // 16 ways, 2048 lines/way, 32 bytes/line

// Each piece of data can go in one of 16 ways
// Which bits do what?
// [ NC  | bank | ...  |  way  |  line  |  byte  ]
//  31 30 29  27        19   16 15     5 4      0

int main(int argc, char** argv) {
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        fprintf(stderr, "Usage: %s [number of iterations]\n", argv[0]);
        fprintf(stdout, "Program will iterate forever if the number of iterations is not specified.");
        return 1;
    }

    uint32_t iterations = 0;
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
    uint8_t* buffer = malloc(L2_SIZE * 4);

    if (!buffer) {
        perror("Unable to allocate buffer. Terminating...");
        return 2;
    }

    for (uint32_t i = 0; i < iterations || argc == 1; i++) {
        for (uint32_t i = 0; i < L2_SIZE * 4; i += LINE_SIZE) {
            buffer[i]++;
        }
    }
    // The below math relies on the L2 being at least 256KB
    double total_kbytes = (L2_SIZE/1024)*4*iterations;
    if (total_kbytes/(1<<20) >= 1)
        fprintf(stdout, "Completed generating %.1fGiB of memory requests.\n", total_kbytes/(1<<20));
    else
        fprintf(stdout, "Completed generating %.1fMiB of memory requests.\n", total_kbytes/(1<<10));

    free(buffer);
    return 0;
}
