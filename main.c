#include "header.h"
#include <time.h>

uint32_t Memory[0x400000];
uint32_t PC;
int32_t R[32];

COUNTING counting;

int main(int argc, char* argv[]) {
    clock_t start = clock();
    char* filename;
    if (argc == 2) {
        filename = argv[1];
    }
    else {
        filename = "Simple4.bin";
    }
  
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("ERROR");
	    exit(EXIT_FAILURE);
    }
    int buffer;
    int index = 0;
    int ret;
    while (1) {
        ret = fread(&buffer, sizeof(buffer), 1, fp);
        if (ret != 1) {  // EOF or error
        break;
        }
        Memory[index] = __builtin_bswap32(buffer);  // Little endian to Big endian
        index++;
    }
  
    for (int i = 0; i < index; i++) {
        printf("Memory [%d] : 0x%08x\n", i, Memory[i]);
    }
  
    R[31] = 0xFFFFFFFF;  // $ra = 0xFFFFFFFF
    R[29] = 0x1000000;  // $sp = 0x1000000
    while(PC != 0xffffffff && index != 0) {
        counting.cycle++;
        if (PC > (index - 1) * 4) {  // Exception : PC points over the end of instruction
        fprintf(stderr, "\nERROR: PC points over the end of instructions\n");
        exit(EXIT_FAILURE);
        }

        /* Instruction Fetch */
        IF(PC);

        switch (counting.format) {
            case 'R' :
                counting.Rcount++;
                break;

            case 'I' :
                counting.Icount++;
                break;

            case 'J' :
                counting.Jcount++;
                break;

            default :
                fprintf(stderr, "ERROR: Instruction has wrong format.\n");
                exit(EXIT_FAILURE);
        }
        printf("\n======================================\n");
    }
    printf("======================================\n");
    printf("======================================\n");
    printf("\n<<End of execution>>\n");

    for (int i = 0; i < 32; i += 2) {
        printf("\nR[%d] : %d   |   R[%d] : %d", i, R[i], i + 1, R[i + 1]);
    }
    printf("\n");
    printf("\nFinal return value R[2] = %d\n", R[2]);
    printf("# of executed instructions : %d\n", counting.Rcount + counting.Icount + counting.Jcount);
    printf("# of executed R-format instructions : %d\n", counting.Rcount);
    printf("# of executed I-format instructions : %d\n", counting.Icount);
    printf("# of executed J-format instructions : %d\n", counting.Jcount);
    printf("# of memory access instructions : %d\n", counting.Memcount);
    printf("# of taken branches : %d\n", counting.takenBranch);
    printf("# of not taken branches : %d\n", counting.nottakenBranch);
    printf("# of clock cycles : %d\n", counting.cycle);
    fclose(fp);
    clock_t end = clock();
    printf("Execution time : %lf\n", (double)(end - start) / CLOCKS_PER_SEC);
    return 0;
}