#include <stdio.h>
#include <stdlib.h>
#include "miner.h"

long int simple_hash(long int number) {
    long int result = (number * BIG_X + BIG_Y) % PRIME;
    return result;
}

void print_blocks(Block *plast_block, int num_wallets) {
    Block *block = NULL;
    int i, j;

    for(i = 0, block = plast_block; block != NULL; block = block->prev, i++) {
        printf("Block number: %d; Target: %ld;    Solution: %ld\n", block->id, block->target, block->solution);
        for(j = 0; j < num_wallets; j++) {
            printf("%d: %d;         ", j, block->wallets[j]);
        }
        printf("\n\n\n");
    }
    printf("A total of %d blocks were printed\n", i);
}

/*
int main(int argc, char *argv[]) {
    long int i, target;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <TARGET>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    target = atol(argv[1]);

    for (i = 0; i < PRIME; i++) {
        fprintf(stdout, "Searching... %6.2f%%\r", 100.0 * i / PRIME);
        if (target == simple_hash(i)) {
            fprintf(stdout, "\nSolution: %ld\n", i);
            exit(EXIT_SUCCESS);
        }
    }
    fprintf(stderr, "\nSearch failed\n");
    exit(EXIT_FAILURE);
}
*/


