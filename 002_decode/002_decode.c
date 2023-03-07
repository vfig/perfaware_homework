#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
enum regname {
    AL, BL, CL, DL,
    AH, BH, CH, DH,
    AX, BX, CX, DX,
    SP, BP, SI, DI,

    NO_REGNAME=-1,
};

static const char *reg_names[] = {
    "AL", "BL", "CL", "DL",
    "AH", "BH", "CH", "DH",
    "AX", "BX", "CX", "DX",
    "SP", "BP", "SI", "DI",
};

enum argt {
    argt_regname, argt_mem, argt_imm,
};

struct insn_port {
    int argt;
    union {
        int regname;
        struct {
            int mem_regname1;
            int mem_regname2;
            int mem_displacement;
            int mem_use_displacement;
        };
        struct {
            int imm_data;
        };
    };
};


struct insn_mov {
    int bits;
    struct insn_port src;
    struct insn_port dst;
};

void fprint_mov_port(FILE *fout, struct insn_port port) {
    switch (port.argt) {
    case argt_regname:
        fputs(reg_names[port.regname], fout);
        break;

    case argt_mem:
        fputs("[", fout);
        if (port.mem_regname1!=NO_REGNAME) {
            fputs(reg_names[port.mem_regname1], fout);
            if (port.mem_regname2!=NO_REGNAME) {
                fputs("+", fout);
                fputs(reg_names[port.mem_regname2], fout);
            }
            if (port.mem_use_displacement && port.mem_displacement) {
                fputs("+", fout);
            }
        }
        if (port.mem_use_displacement) {
            if (port.mem_displacement||port.mem_regname1==NO_REGNAME) {
                fprintf(fout, "%d", port.mem_displacement);
            }
        }
        fputs("]", fout);
        break;

    case argt_imm:
        fprintf(fout, "%d", port.imm_data);
        break;

    default:
        fprintf(stderr, "FUCKED UP\n");
        fprintf(stderr, "  argt: %d\n", port.argt);
        fprintf(stderr, "  regname: %d\n", port.regname);
        fprintf(stderr, "  mem_regname1: %d\n", port.mem_regname1);
        fprintf(stderr, "  mem_regname2: %d\n", port.mem_regname2);
        fprintf(stderr, "  mem_displacement: %d\n", port.mem_displacement);
        fprintf(stderr, "  mem_use_displacement: %d\n", port.mem_use_displacement);
        exit(1);
    }
}

void fprint_mov_insn(FILE *fout, struct insn_mov mov) {
    fputs("mov ", fout);
    fprint_mov_port(fout, mov.dst);
    fputs(", ", fout);
    fprint_mov_port(fout, mov.src);
    fputs("\n", fout);
}

static const int regname_decode_lookup[] = {
    AL, CL, DL, BL, AH, CH, DH, BH,
    AX, CX, DX, BX, SP, BP, SI, DI,
};

int regname_decode(int reg, int w) {
    int index = reg&0x07;
    if (w) index |= 0x08;
    return regname_decode_lookup[index];
}

static const int effaddr_decode_regname1_lookup[] = {
    BX, BX, BP, BP, SI, DI, BP, BX,
};

static const int effaddr_decode_regname2_lookup[] = {
    SI, DI, SI, DI, NO_REGNAME, NO_REGNAME, NO_REGNAME, NO_REGNAME,
};

int effaddr_decode_regname1(int r_m) {
    return effaddr_decode_regname1_lookup[r_m];
}

int effaddr_decode_regname2(int r_m) {
    return effaddr_decode_regname2_lookup[r_m];
}

int effaddr_decode_displacement_bits(int r_m, int mod) {
    if (mod==0 && r_m==6) {
        return 16;
    } else if (mod==1) {
        return 8;
    } else if (mod==2) {
        return 16;
    } else {
        return 0;
    }
}

void decode_stream(FILE *fin, FILE *fout, void (*error)(int where, char *fmt, ...) ) {
    #define READ_BYTE_OR_EOF(intvar) do{ intvar=fgetc(fin); }while(0)
    #define READ_BYTE(intvar) do{ if ((intvar=fgetc(fin))==EOF) { error(ftell(fin), "EOF"); return; } }while(0)
    for (;;) {
        int a;
        READ_BYTE_OR_EOF(a);
        if (a==EOF) {
            break;
        }

        switch (a) {
        // Register/memory to/from register
        case 0x88: case 0x89: case 0x8a: case 0x8b: {
            struct insn_mov mov;
            int w = (a&0x01);
            int d = (a&0x02)>>1;
            mov.bits = w? 16 : 8;
            READ_BYTE(a);
            int mod = (a&0xc0)>>6;
            int reg = (a&0x38)>>3;
            int r_m = (a&0x07);
            int memory_mode = (mod!=3);
            struct insn_port port_reg;
            port_reg.argt = argt_regname;
            port_reg.regname = regname_decode(reg, w);
            struct insn_port port_r_m;
            if (memory_mode) {
                port_r_m.argt = argt_mem;
                port_r_m.mem_regname1 = effaddr_decode_regname1(r_m);
                port_r_m.mem_regname2 = effaddr_decode_regname2(r_m);
                int displacement_bytes = effaddr_decode_displacement_bits(r_m, mod)/8;
                port_r_m.mem_use_displacement = (displacement_bytes!=0);
                int displacement = 0;
                if (displacement_bytes) {
                    int b;
                    READ_BYTE(b);
                    displacement |= b;
                    --displacement_bytes;
                }
                if (displacement_bytes) {
                    int b;
                    READ_BYTE(b);
                    displacement |= (b<<8);
                    --displacement_bytes;
                }
                port_r_m.mem_displacement = displacement;
            } else {
                port_r_m.argt = argt_regname;
                port_r_m.regname = regname_decode(r_m, w);
            }
            if (d) {
                mov.src = port_r_m;
                mov.dst = port_reg;
            } else {
                mov.src = port_reg;
                mov.dst = port_r_m;
            }
            fprint_mov_insn(fout, mov);
        } break;

        // Immediate to register
        case 0xb0: case 0xb1: case 0xb2: case 0xb3:
        case 0xb4: case 0xb5: case 0xb6: case 0xb7:
        case 0xb8: case 0xb9: case 0xba: case 0xbb:
        case 0xbc: case 0xbd: case 0xbe: case 0xbf: {
            struct insn_mov mov;
            int w = (a&0x08);
            int reg = (a&0x03);
            mov.bits = w? 16 : 8;
            struct insn_port port_reg;
            port_reg.argt = argt_regname;
            port_reg.regname = regname_decode(reg, w);
            struct insn_port port_data;
            port_data.argt = argt_imm;
            int data_bytes = mov.bits/8;
            int data = 0;
            if (data_bytes) {
                int b;
                READ_BYTE(b);
                data |= b;
                --data_bytes;
            }
            if (data_bytes) {
                int b;
                READ_BYTE(b);
                data |= (b<<8);
                --data_bytes;
            }
            port_data.imm_data = data;
            mov.src = port_data;
            mov.dst = port_reg;
            fprint_mov_insn(fout, mov);
        } break;

        default:
            error(ftell(fin), "Unknown opcode 0x%02x", a);
            return;

        } /* end switch */
    }
}

void fatal_error(int where, char *fmt, ...) {
    va_list args;
    fprintf(stderr, "FATAL at 0x%08x: ", where);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

int main(int argc, char **argv) {
    decode_stream(stdin, stdout, fatal_error);
    return 0;
}
