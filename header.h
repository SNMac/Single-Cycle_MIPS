#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

typedef struct _CONTROL_SIGNAL {  // Control signals
	bool PCBranch, ALUSrc, RegWrite, MemRead, MemWrite, Branch, BranchNot,
         Zero, Shift, Jump[2], RegDst[2], MemtoReg[2], SignZero;
    char ALUOp;
}CONTROL_SIGNAL;
// Jump == 0) BranchMux, 1) JumpAddr, 2) R[rs]
// RegDst == 0) rt, 1) rd, 2) $31
// MemtoReg == 0) Writedata = ALUresult, 1) Writedata = Readdata, 2) Writedata = PC + 8, 3) Writedata = upperimm

typedef struct _ALU_CONTROL_SIGNAL {  // Control signals
	bool ChkOvfl, Overflow;
    char ALUSig;
}ALU_CONTROL_SIGNAL;


typedef struct _INSTRUCTION {  // Instruction
    uint32_t address;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    int16_t imm;
    uint8_t funct;
}INSTRUCTION;

/* Stages */
void IF(uint32_t PC);  // Instruction Fetch
void ID(uint32_t instruction, uint32_t PCadd4);  // Instruction Decode, register fetch
void EX(int32_t input1, int32_t input2, uint32_t PCadd4, int32_t rs, uint32_t address, uint8_t shamt, uint32_t extimm, uint32_t upperimm);  // EXecute, address calcuate
void MEM(int32_t input1, int32_t input2, int32_t rs, uint32_t BranchAddr, uint32_t JumpAddr, uint32_t PCadd8, uint32_t upperimm);  // MEMory access
void WB(int32_t input1, int32_t input2, int32_t rs, uint32_t BranchAddr, uint32_t JumpAddr, uint32_t PCadd8, uint32_t upperimm);  // Write Back

/* Data units */
uint32_t InstMem(uint32_t Readaddr);  // Instruction memory
int32_t* RegsRead(uint8_t Readreg1, uint8_t Readreg2);  // Registers (ID)
void RegsWrite(uint8_t Writereg, int32_t Writedata);  // Register (WB)
int32_t DataMem(uint32_t Addr, int32_t Writedata);  // Data memory

int32_t ALU(int32_t input1, int32_t input2);  // ALU
int32_t MUX(int32_t input1, int32_t input2, bool signal);  // signal == 0) input1, 1) input2
int32_t MUX_3(int32_t input1, int32_t input2, int32_t input3, bool signal[]);  // signal == 0) input1, 1) input2, 2) input3
int32_t MUX_4(int32_t input1, int32_t input2, int32_t input3, int32_t input4, bool signal[]);  // signal == 0) input1, 1) input2, 2) input3, 3) input4

/* Control units */
void CtrlUnit(uint8_t opcode, uint8_t funct);  // Control unit
void ALUCtrlUnit(uint8_t funct);  // ALU control unit

/* select ALU operation */
char Rformat(uint8_t funct);  // select ALU operation by funct (R-format)
char Iformat(uint8_t opcode);  // select ALU operation by opcode (I-format)

/* Overflow exception */
void OverflowException();  // Overflow exception
