; Program 3: countdown loop (addresses are written as numbers for now)
ADDI R1, R0, 3      ; 0
ADDI R1, R1, -1     ; 1
BEQ  R0, R1, 1      ; 2  if R1 == 0, skip the next instruction
JUMP 1              ; 3
ADDI R5, R0, 9      ; 4
JUMP 5              ; 5  loop here forever
