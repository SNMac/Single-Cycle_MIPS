# Single-Cycle_MIPS
Single Cycle MIPS processor implemented in C

단일 사이클 MIPS 프로세서를 구현한 코드이다.
지원하는 명령어로는
- R형식 중 add, addu, and, jr, nor, or, slt, sltu, sll, srl, sub, subu
- I형식 중 addi, addiu, andi, beq, bne, lui, lw, ori, slti, sltiu
- J형식 중 j, jal, jalr
이다.

아래 데이터패스를 토대로 코드를 작성함
<img src="https://raw.githubusercontent.com/SNMac/Single-Cycle_MIPS/11c16f0bfe7db32b0d3c5b52d6c89709b3a022a4/Single-Cycle%20MIPS.png">
