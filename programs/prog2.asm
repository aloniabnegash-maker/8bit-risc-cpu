; Program 2: LOAD and STORE
ADDI R1, R0, 5
ADDI R2, R0, 7
STORE R2, 3(R0)     ; save 7 into RAM address 3
LOAD  R3, 3(R0)     ; read it back into R3
ADD   R4, R3, R1    ; 7 + 5 = 12
