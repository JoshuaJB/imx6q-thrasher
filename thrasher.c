/**
 * Copyright 2022 Joshua Bakita
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
#define L3_SIZE 12*1024*1024 // 8MiB L2, 4MiB L3

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
        fprintf(stderr, "Usage: %s [s(equential)/r(andom)] [number of iterations]\n", argv[0]);
        fprintf(stdout, "Program will iterate forever if the number of iterations is not specified.\n");
        return 1;
    }

    int is_seq = argc >= 2 ? argv[1][0] != 'r' : 1;

    unsigned long iterations = 0;
    if (argc >= 3) {
        char* status = {'\0'};
        iterations = strtol(argv[2], &status, 10);
        if (iterations == LONG_MIN || iterations == LONG_MAX || status[0] != '\0') {
            perror("Invalid iteration count");
            return 1;
        }
    } else {
        fprintf(stdout, "Infinitely generating %s memory bus traffic...\n",
                is_seq ? "sequential" : "random");
    }

    // The `aligned` attribute on this pads the structure so that it will fill
    // an entire cache line
    struct entry {
        struct entry* prev;
        struct entry* next;
        int data;
    } __attribute__((aligned(LINE_SIZE)));

    // Allocate a buffer meaningfully larger than the L2 such that when
    // iterating through, subsequent access to the same address will miss.
    // We make it generously larger as the L2 does not use true LRU.
    struct entry* buffer = malloc(L3_SIZE * 4);

    if (!buffer) {
        perror("Unable to allocate buffer. Terminating...");
        return 2;
    }

    // Create random walk via shuffled linked list
    if (!is_seq) {
        struct entry* buf_ll = buffer;
        int num_entries = (L3_SIZE * 4)/sizeof(struct entry);
        // Link all entries in sequence
        buf_ll[0].prev = buf_ll + num_entries - 1;
        buf_ll[num_entries - 1].next = buf_ll;
        for (int i = 1; i < num_entries; i++) {
            buf_ll[i - 1].next = buf_ll + i;
            buf_ll[i].prev = buf_ll + i - 1;
        }
        // Shuffle (n - 1 swaps for array of length n)
        for (int i = 0; i < num_entries - 1; i++) {
            int choice = rand() % (num_entries - i) + i;
            struct entry choice_e = buf_ll[choice];
            struct entry curr_e = buf_ll[i];
            struct entry* before_i = curr_e.prev;
            struct entry* after_choice = choice_e.next;
            // after_i != after_choice as we do not allow for 2-entry lists
            struct entry* after_i = curr_e.next;
            struct entry* before_choice = choice_e.prev;
            // Up to 4 unique elements are referred to by the elements
            // connected to `i` and `choice`. Update those (up to) 4 pointers
            // first.
            if (before_i != after_choice) {
                // Point the element before i at the new location
                before_i->next = buf_ll + choice;
                // Point the element after choice back at its new location
                after_choice->prev = buf_ll + i;
            } else {
                // The element before i is the same as that after choice
                // Just swap
                void* temp = after_choice->next;
                after_choice->next = after_choice->prev;
                after_choice->prev = temp;
            }
            if (after_i != before_choice) {
                // Point the element after i to the new location of i
                after_i->prev = buf_ll + choice;
                // Point the element before choice to the new location
                before_choice->next = buf_ll + i;
            } else {
                // The element after i is the same as that before choice
                // Just swap
                void* temp = after_i->next;
                after_i->next = after_i->prev;
                after_i->prev = temp;
            }
            // Now just `i` and `choice` need pointer updates
            buf_ll[choice] = curr_e;
            buf_ll[i] = choice_e;
            // If `choice` pointed to `i` (or vice-versa) the above would have
            // created a cycle. Undo that here.
            if (buf_ll[i].next == buf_ll + i) {
                buf_ll[i].next = buf_ll + choice;
                buf_ll[choice].prev = buf_ll + i;
            }
            if (buf_ll[choice].next == buf_ll + choice) {
                buf_ll[i].prev = buf_ll + choice;
                buf_ll[choice].next = buf_ll + i;
            }
        }
    }

    uint64_t start = get_ms();
    if (is_seq) {
        // Sequential walk
        for (unsigned long i = 0; i < iterations || argc <= 2; i++) {
            for (unsigned long i = 0; i < (L3_SIZE * 4) / LINE_SIZE; i++) {
                buffer[i].data++;
            }
        }
    } else {
        // Random, dependent walk
        struct entry* curr = buffer;
        for (unsigned long i = 0; i < iterations || argc <= 2; i++) {
            do {
                curr->data++;
                curr = curr->next;
            } while (curr != (void*)buffer);
        }
    }
    uint64_t end = get_ms();

    // The below math relies on the L3 being at least 256KB (aka, buffer is always >1MiB)
    double total_kbytes = (L3_SIZE/1024)*4*iterations;
    if (total_kbytes/(1<<20) >= 1)
        fprintf(stdout, "Completed generating %.1fGiB of memory requests", total_kbytes/(1<<20));
    else
        fprintf(stdout, "Completed generating %.1fMiB of memory requests", total_kbytes/(1<<10));
    fprintf(stdout, " in %.2f seconds.\n", (end - start) / 1000.0);

    free(buffer);
    return 0;
}
