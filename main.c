#include "header.h"

uint32_t Memory[0x400000];
uint32_t PC;
int32_t R[32];
int Rcount;  // R-format instruction count
int Icount;  // I-format instruction count
int Jcount;  // J-format instruction count
int Memcount;  // Memory access instruction count
int Branchcount;  // taken Branch count


int main(int argc, char* argv[]) {
  char* filename;
  if (argc == 2) {
    filename = argv[1];
  }
  else {
    filename = "Simple.bin";
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
  
  int cycle = 0;  // to count clock cycle
  for (int i = 0; i < index; i++) {
      printf("Memory [%d] : 0x%08x\n", i, Memory[i]);
  }
  
  R[31] = 0xFFFFFFFF;  // $ra = 0xFFFFFFFF
  R[29] = 0x1000000;  // $sp = 0x1000000
  while(PC != 0xffffffff && index != 0) {
    cycle++;
    if (PC > (index - 1) * 4) {  // Exception : PC points over the end of instruction
      fprintf(stderr, "\nERROR: PC points over the end of instructions\n");
      exit(EXIT_FAILURE);
    }

    /* Instruction Fetch */
    IF(PC);
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
  printf("# of executed instructions : %d\n", Rcount + Icount + Jcount);
  printf("# of executed R-format instructions : %d\n", Rcount);
  printf("# of executed I-format instructions : %d\n", Icount);
  printf("# of executed J-format instructions : %d\n", Jcount);
  printf("# of memory access instructions : %d\n", Memcount);
  printf("# of taken branches : %d\n", Branchcount);
  printf("# of clock cycles : %d\n", cycle);
  fclose(fp);
  return 0;
}