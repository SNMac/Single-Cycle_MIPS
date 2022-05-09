# Single-Cycle_MIPS
Single Cycle MIPS processor implemented in C

단일 사이클 MIPS 프로세서를 구현한 코드이다.
지원하는 명령어로는
- R형식 중 add, addu, and, jr, nor, or, slt, sltu, sll, srl, sub, subu
- I형식 중 addi, addiu, andi, beq, bne, lui, lw, ori, slti, sltiu
- J형식 중 j, jal, jalr
이다.

testbin폴더에 몇가지 테스트 bin파일이 있다.
- simple.bin : 간단한 return 코드
- simple2.bin : 100을 return하는 코드
- simple3.bin : 1부터 100까지 합을 계산하여 반환하는 코드
- simple4.bin : 재귀를 사용해서 1부터 100까지 합을 계산하여 반환하는 코드
- gcd.bin : 0x1298과 0x9387과의 최대공약수를 계산하는 코드
- fib.bin : 10번째 피보나치 수를 계산하는 코드
- fib_jalr.bin : fib.bin에서 jal을 jalr로 변경, 그에 맞게 수정한 코드
- input4.bin : 10000가지 난수 중에서 102번째로 작은 수를 반환하는 코드

아래 데이터패스를 토대로 코드를 작성하였다.
<img src="https://raw.githubusercontent.com/SNMac/Single-Cycle_MIPS/42ad921c014f7f55c5e4eadce06d2c57d12830c2/Single-Cycle%20MIPS.png">
