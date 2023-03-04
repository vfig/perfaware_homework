#include <stdio.h>
#include <stdint.h>

/*
    BYTE 1          BYTE 2
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | : : : : : : : | : : : : : : : |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |  OPCODE   |D|W|MOD| REG | R/M |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    D - 0: source in REG;
        1: dest in REG.
    W - 0: 8 bit;
        1: 16 bit.
    MOD - 11: register mode.

    REG     W=0 W=1
    ---------------
    000:    AL  AX
    001:    CL  CX
    010:    DL  DX
    011:    BL  BX
    100:    AH  SP
    101:    CH  BP
    110:    DH  SI
    111:    BH  DI

                            | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 |
                            +-----------------+-----------------+-----------------+-----------------+-----------------+-----------------+
    reg/mem to/from reg     | 1 0 0 0 1 0 d w | mod reg   r/m   | (DISP_LO)       | (DISP-HI)       |
                            +-----------------+-----------------+-----------------+-----------------+-----------------+-----------------+
    imm to reg/mem          | 1 1 0 0 0 1 1 w | mod 0 0 0 r/m   | (DISP_LO)       | (DISP-HI)       | data            | data if w=1     |
                            +-----------------+-----------------+-----------------+-----------------+-----------------+-----------------+
    imm to reg              | 1 0 1 1 w reg   | data            | data if w=1     |
                            +-----------------+-----------------+-----------------+
    mem to accum            | 1 0 1 0 0 0 0 w | addr-lo         | addr-hi         |
                            +-----------------+-----------------+-----------------+
    accum to mem            | 1 0 1 0 0 0 1 w | addr-lo         | addr-hi         |
                            +-----------------+-----------------+-----------------+-----------------+
    reg/mem to segment reg  | 1 0 0 0 1 1 1 0 | mod 0 SR  r/m   | (DISP_LO)       | (DISP-HI)       |
                            +-----------------+-----------------+-----------------+-----------------+
    segment reg to reg/mem  | 1 0 0 0 1 1 0 0 | mod 0 SR  r/m   | (DISP_LO)       | (DISP-HI)       |
                            +-----------------+-----------------+-----------------+-----------------+

*/
enum reg {
    AL, BL, CL, DL,
    AH, BH, CH, DH,
    AX, BX, CX, DX,
    SP, BP, SI, DI,
};

static const char *reg_names[] = {
    "AL", "BL", "CL", "DL",
    "AH", "BH", "CH", "DH",
    "AX", "BX", "CX", "DX",
    "SP", "BP", "SI", "DI",
};

enum argt {
    argt_reg, argt_mem, argt_imm,
};

struct insn_mov {
    int bits;
    int src_argt;
    union {
        int src_reg;
    };
    int dst_argt;
    union {
        int dst_reg;
    };
};

void fprint_mov_insn(FILE *fout, struct insn_mov mov) {
    fputs("mov ", fout);
    if (mov.dst_argt==argt_reg) {
        fputs(reg_names[mov.dst_reg], fout);
    }
    fputs(", ", fout);
    if (mov.src_argt==argt_reg) {
        fputs(reg_names[mov.src_reg], fout);
    }
    fputs("\n", fout);
}

static const int decode_reg_lookup[] = {
    AL, CL, DL, BL, AH, CH, DH, BH,
    AX, CX, DX, BX, SP, BP, SI, DI,
};

int decode_reg(int reg, int w) {
    int index = reg&0x07;
    if (w) index |= 0x08;
    return decode_reg_lookup[index];
}

void decode_stream(FILE *fin, FILE *fout, void (*error)(char *message) ) {
    #define READ_BYTE_OR_EOF(intvar) do{ intvar=fgetc(fin); }while(0)
    #define READ_BYTE(intvar) do{ if ((intvar=fgetc(fin))==EOF) { error("EOF"); return; } }while(0)
    for (;;) {
        int a;
        READ_BYTE_OR_EOF(a);
        if (a==EOF) break;

        switch (a) {

        case 0x88:
        case 0x89:
        case 0x8a:
        case 0x8b: {
            struct insn_mov mov;
            int w = (a&0x01);
            int d = (a&0x02)>>1;
            mov.bits = w? 16 : 8;
            READ_BYTE(a);
            int mod = (a&0xc0)>>6;
            if (mod!=3) {
                error("Unknown mod");
                return;
            }
            mov.src_argt = argt_reg;
            mov.dst_argt = argt_reg;
            int reg = (a&0x38)>>3;
            int r_m = (a&0x07);
            if (d) {
                mov.src_reg = decode_reg(r_m, w);
                mov.dst_reg = decode_reg(reg, w);
            } else {
                mov.src_reg = decode_reg(reg, w);
                mov.dst_reg = decode_reg(r_m, w);
            }
            fprint_mov_insn(fout, mov);
        } break;

        default:
            error("Unknown opcode");
            return;

        } /* end switch */
    }
}

void fatal_error(char *message) {
    fprintf(stderr, "Fatal: %s", message);
    exit(1);
}

int main(int argc, char **argv) {
    decode_stream(stdin, stdout, fatal_error);
}
