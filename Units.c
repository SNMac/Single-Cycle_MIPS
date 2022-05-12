#include "header.h"

CONTROL_SIGNAL ctrlSig;
ALU_CONTROL_SIGNAL ALUctrlSig;
INSTRUCTION inst;

extern uint32_t Memory[0x400000];
extern uint32_t PC;
extern int32_t R[32];
extern int Rcount;  // R-format instruction count
extern int Icount;  // I-format instruction count
extern int Jcount;  // J-format instruction count
extern int Memcount;  // Memory access instruction count
extern int takenBranch;  // taken branch count
extern int nottakenBranch;  // not taken branch count

/*============================Stages============================*/

// Instruction Fetch
void IF(uint32_t PC) {
    //           PC
    printf("\nPC : 0x%08x\n", PC);
    uint32_t IF_return = InstMem(PC);
    ID(IF_return, PC + 4);  // go to next stage
    return;
}
// Instruction Decode
void ID(uint32_t instruction, uint32_t PCadd4) {
    //           Instruction,          PC + 4
    printf("Instruction : 0x%08x\n", instruction);
    memset(&inst, 0, sizeof(INSTRUCTION));
    inst.address = instruction & 0x3ffffff;  // for J-format
    inst.opcode = (instruction & 0xfc000000) >> 26;
    inst.rs = (instruction & 0x03e00000) >> 21;
    inst.rt = (instruction & 0x001f0000) >> 16;
    inst.rd = (instruction & 0x0000f800) >> 11;
    inst.shamt = (instruction & 0x000007c0) >> 6;  // for sll, srl
    inst.imm = instruction & 0x0000ffff;  // for I-format
    inst.funct = inst.imm & 0x003f;  // for R-format

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);
    ALUCtrlUnit(inst.funct);

    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Register Fetch
    int* ID_return = RegsRead(inst.rs, inst.rt);
    EX(ID_return[0], ID_return[1], PCadd4, R[inst.rs], inst.address, inst.shamt, extimm, zeroimm << 16);  // go to next stage
    return;
}

// Execute
void EX(int32_t input1, int32_t input2, uint32_t PCadd4, int32_t rs, uint32_t address, uint8_t shamt, uint32_t extimm, uint32_t upperimm) {
    //          R[rs],          R[rt],           PC + 4,         rs           address,         shamt,       extened imm         upperimm

    // Execute operation
    int32_t EX_return[2];
    int32_t ALUinput1 = MUX(input1, shamt, ctrlSig.Shift);
    int32_t ALUinput2 = MUX(input2, extimm, ctrlSig.ALUSrc);
    EX_return[0] = ALU(ALUinput1, ALUinput2);
    EX_return[1] = input2;

    // Address calculation
    uint32_t BranchAddr = PCadd4 + (extimm << 2);
    uint32_t JumpAddr = (PCadd4 & 0xf0000000) | (address << 2);
    MEM(EX_return[0], EX_return[1], rs, BranchAddr, JumpAddr, PCadd4 + 4, upperimm);  // go to next stage
    return;
}

// Memory Access
void MEM(int32_t input1, int32_t input2, int32_t rs, uint32_t BranchAddr, uint32_t JumpAddr, uint32_t PCadd8, uint32_t upperimm) {
    //        ALU result,         R[rt],         rs           BranchAddr,          JumpAddr,          PC + 8           upperimm

    int32_t MEM_return = DataMem(input1, input2);
    WB(MEM_return, input1, rs, BranchAddr, JumpAddr, PCadd8, upperimm);
    return;
}

// Write Back
void WB(int32_t input1, int32_t input2, int32_t rs, uint32_t BranchAddr, uint32_t JumpAddr, uint32_t PCadd8, uint32_t upperimm) {
    //        Read data,      ALU result,       rs           BranchAddr,          JumpAddr,          PC + 8,          upperimm
    
    // select Write data
    int32_t MemtoRegMUX = MUX_4(input2, input1, PCadd8, upperimm, ctrlSig.MemtoReg);

    // update register
    uint8_t RegDstMUX = MUX_3(inst.rt, inst.rd, 31, ctrlSig.RegDst);
    RegsWrite(RegDstMUX, MemtoRegMUX);

    // select PC value
    ctrlSig.PCBranch = (ctrlSig.BranchNot & !ctrlSig.Zero) | (ctrlSig.Branch & ctrlSig.Zero);
    uint32_t PCBranchMUX = MUX(PC + 4, BranchAddr, ctrlSig.PCBranch);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, rs, ctrlSig.Jump);
    PC = JumpMUX;
    if (ctrlSig.Branch | ctrlSig.BranchNot) {
        if (ctrlSig.PCBranch) {
            takenBranch++;
        }
        else {
            nottakenBranch++;
        }
    }
    return;
}

/*============================Data units============================*/

// [Instruction memory]
uint32_t InstMem(uint32_t Readaddr) {
    uint32_t Instruction = Memory[Readaddr / 4];
    return Instruction;
}

// [Registers]
int32_t* RegsRead(uint8_t Readreg1, uint8_t Readreg2) {
        static int32_t Readdata[2];
        Readdata[0] = R[Readreg1];
        Readdata[1] = R[Readreg2];
        return Readdata;
}
void RegsWrite(uint8_t Writereg, int32_t Writedata) {
    if (ctrlSig.RegWrite == 1) {  // RegWrite asserted
        if (Writereg == 0 && Writedata != 0) {
            fprintf(stderr, "WARNING: Cannot change value at $0\n");
            return;
        }
        R[Writereg] = Writedata;  // GPR write enabled
        printf("R[%d] = 0x%x (%d)\n", Writereg, R[Writereg], R[Writereg]);
        return;
    }
    else {  // RegWrite De-asserted
        return;  // GPR write disabled
    }
}
// [Data memory]
int32_t DataMem(uint32_t Addr, int32_t Writedata) {
    int32_t Readdata = 0;
    if (ctrlSig.MemRead == 1 && ctrlSig.MemWrite == 0) {  // MemRead asserted, MemWrite De-asserted
        if (Addr > 0x1000000) {  // loading outside of memory
            fprintf(stderr, "ERROR: Accessing outside of memory\n");
            exit(EXIT_FAILURE);
        }
        Readdata = Memory[Addr / 4];  // Memory read port return load value
        printf("Memory[0x%08x] load -> 0x%x (%d)\n", Addr, Memory[Addr / 4], Memory[Addr / 4]);
        Memcount++;
    }
    else if (ctrlSig.MemRead == 0 && ctrlSig.MemWrite == 1) {  // MemRead De-asserted, MemWrite asserted
        if (Addr > 0x1000000) {  // loading outside of memory
            fprintf(stderr, "ERROR: Accessing outside of memory\n");
            exit(EXIT_FAILURE);
        }
        Memory[Addr / 4] = Writedata;  // Memory write enabled
        printf("Memory[0x%08x] <- store 0x%x (%d)\n", Addr, Memory[Addr / 4], Memory[Addr / 4]);
        Memcount++;
    }
    return Readdata;
}

// [ALU]
int32_t ALU(int32_t input1, int32_t input2) {
    int32_t ALUresult = 0;
    switch (ALUctrlSig.ALUSig) {
        case '+' :  // add
            ALUresult = input1 + input2;
            break;

        case '-' :  // subtract
            ALUresult = input1 - input2;
            if (ALUresult == 0) {
                ctrlSig.Zero = 1;
            }
            else {
                ctrlSig.Zero = 0;
            }
            break;

        case '&' :  // AND
            ALUresult = input1 & input2;
            break;

        case '|' :  // OR
            ALUresult = input1 | input2;
            break;

        case '~' :  // NOR
            ALUresult = ~(input1 | input2);
            break;
        
        case '<' :  // set less than
            if ( (input1 & 0x80000000) == (input2 & 0x80000000) ) {  // same sign
                if (input1 < input2) {
                    ALUresult = 1;
                }
                else {
                    ALUresult = 0;
                }
            }
            else if ( (input1 & 0x80000000) == 1 ) {  // different sign
                ALUresult = 1;
            }
            else {
                ALUresult = 0;
            }
            break;

        case '{' :  // sll
            ALUresult = input2 << input1;
            break;

        case '}' :  // srl
            ALUresult = input2 >> input1;
            break;   
    }
    if ( ((input1 & 0x80000000) == (input2 & 0x80000000)) && ((ALUresult & 0x80000000) != (input1 & 0x80000000)) ) {
        // same sign input = different sign output
        if (ALUctrlSig.ChkOvfl == 1) {  // if instruction == add || addi || sub
            OverflowException();
        }
    }
    return ALUresult;
}

// [MUX with 2 input]
int32_t MUX(int32_t input1, int32_t input2, bool signal) {
    if (signal == 0) {
        return input1;
    }
    else {
        return input2;
    }
}

// [MUX with 3 input]
int32_t MUX_3(int32_t input1, int32_t input2, int32_t input3, bool signal[]) {
    if (signal[1] == 0 && signal[0] == 0) {
        return input1;
    }
    else if (signal[1] == 0 && signal[0] == 1) {
        return input2;
    }
    else {
        return input3;
    }
}

int32_t MUX_4(int32_t input1, int32_t input2, int32_t input3, int32_t input4, bool signal[]) {
    if (signal[1] == 0 && signal[0] == 0) {
        return input1;
    }
    else if (signal[1] == 0 && signal[0] == 1) {
        return input2;
    }
    else if (signal[1] == 1 && signal[0] == 0){
        return input3;
    }
    else {
        return input4;
    }
}

/*============================Control units============================*/

// (Control unit)
void CtrlUnit(uint8_t opcode, uint8_t funct) {
    memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
    switch (opcode) {  // considered don't care as 0
        case 0x0 :  // R-format
            Rcount++;
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 1;  // Write register = rd
            ctrlSig.ALUSrc = 0;  // ALU input2 = rt
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;  // Write data = ALU result
            ctrlSig.RegWrite = 1;  // GPR write enabled
            ctrlSig.MemRead = 0;  // Memory read disabled
            ctrlSig.MemWrite = 0;  // Memory write enabled
            ctrlSig.BranchNot = 0;  // opcode != bne
            ctrlSig.Branch = 0;  // opcode != beq
            ctrlSig.ALUOp = '0';  // select operation according to funct
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            if (funct == 0x08) {  // jr
                ctrlSig.RegWrite = 0;
                ctrlSig.Jump[1] = 1; ctrlSig.Jump[0] = 0;
            }
            else if (funct == 0x9) {  // jalr
                ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 0;  // Write data = PC + 8
                ctrlSig.Jump[1] = 1; ctrlSig.Jump[0] = 0;
            }
            else if (funct == 0x00 || funct == 0x02) {  // sll, srl
                ctrlSig.Shift = 1;  // ALU input1 = shamt
            }
            break;

        case 0x8 :  // addi
            Icount++;
            printf("format : I   |   addi $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';  // select operation according to opcode
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        case 0x9 :  // addiu
            Icount++;
            printf("format : I   |   addiu $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        case 0xc :  // andi
            Icount++;
            printf("format : I   |   andi $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 1;
            break;

        case 0x4 :  // beq
            Icount++;
            printf("format : I   |   beq $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.ALUSrc = 0;
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 1;
            ctrlSig.ALUOp = 'B';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x5 :  // bne
            Icount++;
            printf("format : I   |   bne $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.ALUSrc = 0;
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 1;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'B';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x2 :  // j
            Jcount++;
            printf("format : J   |   j 0x%08x\n", inst.address);
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'X';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 1;
            break;

        case 0x3 :  // jal
            Jcount++;
            printf("format : J   |   jal 0x%08x\n", inst.address);
            ctrlSig.RegWrite = 1;
            ctrlSig.RegDst[1] = 1; ctrlSig.RegDst[0] = 0;
            ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.ALUOp = 'X';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 1;
            break;
        
        case 0xf :  // lui
            Icount++;
            printf("format : I   |   lui $%d, 0x%x\n", inst.rt, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 1;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x23 :  // lw
            Icount++;
            printf("format : I   |   lw $%d, 0x%x($%d)\n", inst.rt, inst.imm, inst.rs);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 1;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 1;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'l';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        case 0xd :  // ori
            Icount++;
            printf("format : I   |   ori $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 1;
            break;

        case 0xa :  // slti
            Icount++;
            printf("format : I   |   slti $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        case 0xb :  // sltiu
            Icount++;
            printf("format : I   |   sltiu $%d, $%d, 0x%x\n", inst.rt, inst.rs, inst.imm);
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 'i';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        case 0x2B :  // sw
            Icount++;
            printf("format : I   |   sw $%d, 0x%x($%d)\n", inst.rt, inst.imm, inst.rs);
            ctrlSig.ALUSrc = 1; 
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 1;
            ctrlSig.BranchNot = 0;
            ctrlSig.Branch = 0;
            ctrlSig.ALUOp = 's';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            ctrlSig.SignZero = 0;
            break;

        default :
            fprintf(stderr, "ERROR: Instruction has wrong opcode.\n");
            exit(EXIT_FAILURE);
    }
    return;
}

// (ALU control unit)
void ALUCtrlUnit(uint8_t funct) {
    memset(&ALUctrlSig, 0, sizeof(ALU_CONTROL_SIGNAL));
    switch (ctrlSig.ALUOp) {  // 0, i, B, l, s, X
        case '0' :  // select operation according to funct
            ALUctrlSig.ALUSig = Rformat(funct);
            return;

        case 'i' :  // select operation according to opcode
            ALUctrlSig.ALUSig = Iformat(inst.opcode);
            return;

        case 'l' :  // select +
            ALUctrlSig.ALUSig = '+';
            return;

        case 's' :  // select +
            ALUctrlSig.ALUSig = '+';
            return;

        case 'B' :  // select bcond generation function
            ALUctrlSig.ALUSig = '-';
            return;
        
        case 'X' :  // don't care
            ALUctrlSig.ALUSig = '+';
            return;

        default :
            fprintf(stderr, "ERROR: Instruction has wrong funct\n");
            exit(EXIT_FAILURE);
            
    }
}

/*============================Select ALU operation============================*/

char Rformat(uint8_t funct) {  // select ALU operation by funct (R-format)
    switch (funct) {
        case 0x20 :  // add
            printf("format : R   |   add $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            ALUctrlSig.ChkOvfl = 1;
            return '+';

        case 0x21 :  // addu
            printf("format : R   |   addu $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '+';

        case 0x24 :  // and
            printf("format : R   |   and $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '&';

        case 0x08 :  // jr
            printf("format : R   |   jr $%d\n", inst.rs);
            return 'X';
        
        case 0x9 :  // jalr
            printf("format : R   |   jalr $%d\n", inst.rs);
            return 'X';

        case 0x27 :  // nor
            printf("format : R   |   nor $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '~';

        case 0x25 :  // or
            printf("format : R   |   or $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '|';

        case 0x2a :  // slt
            printf("format : R   |   slt $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '<';

        case 0x2b :  // sltu
            printf("format : R   |   sltu $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '<';

        case 0x00 :  // sll
            printf("format : R   |   sll $%d, $%d, %d\n", inst.rd, inst.rt, inst.shamt);
            
            return '{';

        case 0x02 :  // srl
            printf("format : R   |   srl $%d, $%d, %d\n", inst.rd, inst.rt, inst.shamt);
            return '}';

        case 0x22 :  // sub
            printf("format : R   |   sub $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            ALUctrlSig.ChkOvfl = 1;
            return '-';

        case 0x23 :  // subu
            printf("format : R   |   subu $%d, $%d, $%d\n", inst.rd, inst.rs, inst.rt);
            return '-';

        default:
            fprintf(stderr, "ERROR: Instruction has wrong funct\n");
            exit(EXIT_FAILURE);
    }
}

char Iformat(uint8_t opcode) {  // select ALU operation by opcode (I-format)
    switch (opcode) {
        case 0x8 :  // addi
            ALUctrlSig.ChkOvfl = 1;
            return '+';

        case 0x9 :  // addiu
            return '+';

        case 0xc :  // andi
            return '&';
        
        case 0xf :  // lui
            return 'X';

        case 0xd :  // ori
            return '|';

        case 0xa :  // slti
            return '<';

        case 0xb :  // sltiu
            return '<';

        default:
            fprintf(stderr, "ERROR: Instruction has wrong opcode\n");
            exit(EXIT_FAILURE);
    }
}

/*============================Exception============================*/

// Overflow exception
void OverflowException() {  // check overflow in signed instruction
    fprintf(stderr, "ERROR: Arithmetic overflow occured\n");
    exit(EXIT_FAILURE);
}
