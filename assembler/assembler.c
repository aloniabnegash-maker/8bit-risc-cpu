/*
 * assembler.c - a tiny assembler for the 8-bit Logisim RISC CPU (Stage 1)
 *
 * Usage:   assembler program.asm [output.txt]
 *
 * Reads one instruction per line, for example:
 *     ADDI R1, R0, 5      ; comments start with a semicolon
 *     STORE R2, 3(R0)
 * Prints the machine code on the screen. If you give an output file, it also
 * writes the words in the "v2.0 raw" format that Logisim's ROM can load.
 *
 * Instruction formats (bit 15 on the left):
 *   R-type  ADD/SUB/AND/OR  Rd, Rs1, Rs2   opcode | rd | rs1 | rs2 | 000
 *   I-type  ADDI Rd, Rs1, imm              opcode | rd | rs1 | imm6
 *   Memory  LOAD/STORE Rd, imm(Rs1)        opcode | rd | rs1 | imm6
 *   Branch  BEQ Ra, Rb, offset             opcode | ra | rb  | offset6
 *   Jump    JUMP address                   opcode | 12-bit address
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORDS 256   /* the ROM has 256 words */
#define MAX_LINE  128

/* The five "shapes" an instruction can have. */
typedef enum { FMT_R, FMT_I, FMT_MEM, FMT_BEQ, FMT_JUMP } Format;

typedef struct {
    const char *name;
    int opcode;
    Format fmt;
} Instr;

/* To add a new instruction later, add one line here. */
static const Instr table[] = {
    { "ADD",   0x0, FMT_R    },
    { "SUB",   0x1, FMT_R    },
    { "AND",   0x2, FMT_R    },
    { "OR",    0x3, FMT_R    },
    { "ADDI",  0x4, FMT_I    },
    { "LOAD",  0x5, FMT_MEM  },
    { "STORE", 0x6, FMT_MEM  },
    { "BEQ",   0x7, FMT_BEQ  },
    { "JUMP",  0x8, FMT_JUMP },
};
#define TABLE_SIZE (sizeof(table) / sizeof(table[0]))

/* Look up an instruction name in the table. Returns NULL if not found. */
static const Instr *find_instr(const char *name)
{
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        if (strcmp(table[i].name, name) == 0) {
            return &table[i];
        }
    }
    return NULL;
}

static int reg_ok(int r) { return r >= 0 && r <= 7; }
static int imm_ok(int v) { return v >= -32 && v <= 31; }   /* fits in 6 bits */

/* Print an error message and return -1 (our "this line failed" signal). */
static int bad(int line_no, const char *msg)
{
    fprintf(stderr, "line %d: %s\n", line_no, msg);
    return -1;
}

/*
 * Turn one line of assembly into a 16-bit machine word.
 * Returns the word (0 to 65535), or -1 if the line has an error.
 */
static int encode_line(const char *line, int line_no)
{
    char name[16];
    int a, b, c;

    if (sscanf(line, "%15s", name) != 1) {
        return bad(line_no, "empty line");
    }
    const Instr *ins = find_instr(name);
    if (ins == NULL) {
        return bad(line_no, "unknown instruction");
    }
    int op = ins->opcode;

    switch (ins->fmt) {
    case FMT_R:     /* ADD R3, R1, R2 */
        if (sscanf(line, "%*s R%d, R%d, R%d", &a, &b, &c) != 3)
            return bad(line_no, "expected: NAME Rd, Rs1, Rs2");
        if (!reg_ok(a) || !reg_ok(b) || !reg_ok(c))
            return bad(line_no, "registers must be R0 to R7");
        return (op << 12) | (a << 9) | (b << 6) | (c << 3);

    case FMT_I:     /* ADDI R1, R0, 5 */
        if (sscanf(line, "%*s R%d, R%d, %d", &a, &b, &c) != 3)
            return bad(line_no, "expected: ADDI Rd, Rs1, imm");
        if (!reg_ok(a) || !reg_ok(b))
            return bad(line_no, "registers must be R0 to R7");
        if (!imm_ok(c))
            return bad(line_no, "immediate must be -32 to 31");
        return (op << 12) | (a << 9) | (b << 6) | (c & 0x3F);

    case FMT_MEM:   /* LOAD R3, 3(R0)   a = Rd, b = imm, c = Rs1 */
        if (sscanf(line, "%*s R%d, %d(R%d)", &a, &b, &c) != 3)
            return bad(line_no, "expected: NAME Rd, imm(Rs1)");
        if (!reg_ok(a) || !reg_ok(c))
            return bad(line_no, "registers must be R0 to R7");
        if (!imm_ok(b))
            return bad(line_no, "offset must be -32 to 31");
        return (op << 12) | (a << 9) | (c << 6) | (b & 0x3F);

    case FMT_BEQ:   /* BEQ R0, R1, 1   a and b are compared, c is the offset */
        if (sscanf(line, "%*s R%d, R%d, %d", &a, &b, &c) != 3)
            return bad(line_no, "expected: BEQ Ra, Rb, offset");
        if (!reg_ok(a) || !reg_ok(b))
            return bad(line_no, "registers must be R0 to R7");
        if (!imm_ok(c))
            return bad(line_no, "offset must be -32 to 31");
        return (op << 12) | (a << 9) | (b << 6) | (c & 0x3F);

    case FMT_JUMP:  /* JUMP 1 */
        if (sscanf(line, "%*s %d", &a) != 1)
            return bad(line_no, "expected: JUMP address");
        if (a < 0 || a > 255)
            return bad(line_no, "address must be 0 to 255");
        return (op << 12) | a;
    }
    return bad(line_no, "internal error");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s program.asm [output.txt]\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (in == NULL) {
        perror(argv[1]);
        return 1;
    }

    int words[MAX_WORDS];
    int count = 0, errors = 0, line_no = 0;
    char line[MAX_LINE];

    while (fgets(line, sizeof line, in) != NULL) {
        line_no++;

        /* Cut off any comment (everything after a semicolon). */
        char *semi = strchr(line, ';');
        if (semi != NULL) {
            *semi = '\0';
        }

        /* Make everything upper case so "add r1, r0, 5" also works. */
        for (char *q = line; *q != '\0'; q++) {
            *q = (char)toupper((unsigned char)*q);
        }

        /* Skip leading spaces, and skip lines that are now empty. */
        char *start = line;
        while (isspace((unsigned char)*start)) {
            start++;
        }
        if (*start == '\0') {
            continue;
        }

        if (count >= MAX_WORDS) {
            fprintf(stderr, "line %d: program is longer than %d instructions\n",
                    line_no, MAX_WORDS);
            errors++;
            break;
        }

        int word = encode_line(start, line_no);
        if (word < 0) {
            errors++;
        } else {
            words[count++] = word;
        }
    }
    fclose(in);

    if (errors > 0) {
        fprintf(stderr, "%d error(s), nothing written\n", errors);
        return 1;
    }

    /* Show the result on the screen. */
    for (int i = 0; i < count; i++) {
        printf("%3d: %04X\n", i, words[i]);
    }

    /* Optionally save it for Logisim. */
    if (argc >= 3) {
        FILE *out = fopen(argv[2], "w");
        if (out == NULL) {
            perror(argv[2]);
            return 1;
        }
        fprintf(out, "v2.0 raw\n");
        for (int i = 0; i < count; i++) {
            fprintf(out, "%04x\n", words[i]);
        }
        fclose(out);
        printf("wrote %d words to %s\n", count, argv[2]);
    }
    return 0;
}
