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
    "al", "bl", "cl", "dl",
    "ah", "bh", "ch", "dh",
    "ax", "bx", "cx", "dx",
    "sp", "bp", "si", "di",
};

enum argt {
    argt_regname, argt_regdisp, argt_imm, argt_mem,
};

struct insn_port {
    int argt;
    union {
        int regname;
        struct {
            int regdisp_regname1;
            int regdisp_regname2;
            int16_t regdisp_displacement;
        };
        struct {
            uint16_t imm_data;
        };
        struct {
            uint16_t mem_addr;
        };
    };
};

enum insn_port_select {
    select_port_reg,
    select_port_r_m,
    select_port_imm,
    select_port_mem,
};

struct insn_mov {
    int bits;
    struct insn_port src;
    struct insn_port dst;
};

void fprint_mov_port(FILE *fout, struct insn_port port, int bits) {
    switch (port.argt) {
    case argt_regname:
        fputs(reg_names[port.regname], fout);
        break;

    case argt_regdisp:
        fputs("[", fout);
        if (port.regdisp_regname1==NO_REGNAME) {
            fprintf(fout, "%d", port.regdisp_displacement);
        } else {
            fputs(reg_names[port.regdisp_regname1], fout);
            if (port.regdisp_regname2!=NO_REGNAME) {
                fputs("+", fout);
                fputs(reg_names[port.regdisp_regname2], fout);
            }
            if (port.regdisp_displacement) {
                if (port.regdisp_displacement>0) {
                    fprintf(fout, "+%d", port.regdisp_displacement);
                } else if (port.regdisp_displacement<0) {
                    fprintf(fout, "%d", port.regdisp_displacement);
                }
            }
        }
        fputs("]", fout);
        break;

    case argt_imm:
        switch (bits) {
        case 8: fputs("byte ", fout); break;
        case 16: fputs("word ", fout); break;
        }
        fprintf(fout, "%d", port.imm_data);
        break;

    case argt_mem:
        fputs("[", fout);
        fprintf(fout, "%d", port.mem_addr);
        fputs("]", fout);
        break;

    default:
        fprintf(stderr, "FUCKED UP\n");
        abort();
    }
}

void fprint_mov_insn(FILE *fout, struct insn_mov mov) {
    fputs("mov ", fout);
    fprint_mov_port(fout, mov.dst, mov.bits);
    fputs(", ", fout);
    fprint_mov_port(fout, mov.src, mov.bits);
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
    #define FAIL(fmt, ...) do{ error(ftell(fin), fmt, ##__VA_ARGS__); return; }while(0)
    #define READ_BYTE_OR_EOF(intvar) do{ intvar=fgetc(fin); }while(0)
    #define READ_BYTE(intvar) do{ if ((intvar=fgetc(fin))==EOF) FAIL("unexpected eof"); }while(0)
    for (;;) {
        int w;
        int d;
        int mod;
        int reg;
        int r_m;
        int use_mod;
        int use_reg;
        int use_r_m;
        int use_imm;
        int use_mem;
        int select_src;
        int select_dst;

        int a;
        READ_BYTE_OR_EOF(a);
        if (a==EOF) break;
        switch (a) {
        // Register/memory to/from register
        case 0x88: case 0x89: case 0x8a: case 0x8b:
            w = (a&0x01);
            d = (a&0x02)>>1;
            use_mod = 1;
            use_reg = 1;
            use_r_m = 1;
            use_imm = 0;
            use_mem = 0;
            if (d) {
                select_src = select_port_r_m;
                select_dst = select_port_reg;
            } else {
                select_src = select_port_reg;
                select_dst = select_port_r_m;
            }
            goto decode_mov;

        // Immediate to register/memory
        case 0xc6: case 0xc7:
            w = (a&0x01);
            use_mod = 1;
            use_reg = 0;
            use_r_m = 1;
            use_imm = 1;
            use_mem = 0;
            select_src = select_port_imm;
            select_dst = select_port_r_m;
            goto decode_mov;

        // Immediate to register
        case 0xb0: case 0xb1: case 0xb2: case 0xb3:
        case 0xb4: case 0xb5: case 0xb6: case 0xb7:
        case 0xb8: case 0xb9: case 0xba: case 0xbb:
        case 0xbc: case 0xbd: case 0xbe: case 0xbf:
            w = (a&0x08);
            reg = (a&0x03);
            use_mod = 0;
            use_reg = 1;
            use_r_m = 0;
            use_imm = 1;
            use_mem = 0;
            select_src = select_port_imm;
            select_dst = select_port_reg;
            goto decode_mov;

        // Memory to accumulator
        case 0xa0: case 0xa1:
            w = (a&0x01);
            reg = 0;
            use_mod = 0;
            use_reg = 1;
            use_r_m = 0;
            use_imm = 0;
            use_mem = 1;
            select_src = select_port_mem;
            select_dst = select_port_reg;
            goto decode_mov;

        // Accumulator to memory
        case 0xa2: case 0xa3:
            w = (a&0x01);
            reg = 0;
            use_mod = 0;
            use_reg = 1;
            use_r_m = 0;
            use_imm = 0;
            use_mem = 1;
            select_src = select_port_reg;
            select_dst = select_port_mem;
            goto decode_mov;

        // ---- MOV decode ----
        decode_mov: {
            struct insn_mov mov;
            mov.bits = w? 16 : 8;
            if (use_mod) {
                READ_BYTE(a);
                mod = (a&0xc0)>>6;
                reg = (a&0x38)>>3;
                r_m = (a&0x07);
            }
            // DIRECT ADDRESS MODE: detour via use_mem instead of use_r_m:
            if (use_mod && use_r_m && r_m==6 && mod==0) {
                use_r_m = 0;
                use_mem = 1;
                if (select_src==select_port_r_m) select_src = select_port_mem;
                if (select_dst==select_port_r_m) select_dst = select_port_mem;
            }
            struct insn_port port_reg;
            struct insn_port port_r_m;
            struct insn_port port_imm;
            struct insn_port port_mem;
            if (use_reg) {
                port_reg.argt = argt_regname;
                port_reg.regname = regname_decode(reg, w);
            }
            if (use_r_m) {
                int memory_mode = (mod!=3);
                if (memory_mode) {
                    port_r_m.argt = argt_regdisp;
                    port_r_m.regdisp_regname1 = effaddr_decode_regname1(r_m);
                    port_r_m.regdisp_regname2 = effaddr_decode_regname2(r_m);
                    int displacement_bytes = effaddr_decode_displacement_bits(r_m, mod)/8;
                    int16_t displacement = 0;
                    if (displacement_bytes) {
                        // Low 8 bits
                        int b;
                        READ_BYTE(b);
                        displacement |= b;
                        --displacement_bytes;
                    }
                    if (displacement_bytes) {
                        // High 8 bits
                        int b;
                        READ_BYTE(b);
                        displacement |= (b<<8);
                        --displacement_bytes;
                    } else {
                        // Sign-extend 8-bit displacement
                        if (displacement&0x80)
                            displacement |= 0xFF00;
                    }
                    port_r_m.regdisp_displacement = displacement;
                } else {
                    port_r_m.argt = argt_regname;
                    port_r_m.regname = regname_decode(r_m, w);
                }
            }
            if (use_mem) {
                port_mem.argt = argt_mem;
                uint16_t addr = 0;
                int b;
                READ_BYTE(b);
                addr |= b;
                READ_BYTE(b);
                addr |= (b<<8);
                port_mem.mem_addr = addr;
            }
            if (use_imm) {
                port_imm.argt = argt_imm;
                int data_bytes = w?2:1;
                uint16_t data = 0;
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
                port_imm.imm_data = data;
            }
            switch (select_src) {
            case select_port_reg: mov.src = port_reg; break;
            case select_port_r_m: mov.src = port_r_m; break;
            case select_port_imm: mov.src = port_imm; break;
            case select_port_mem: mov.src = port_mem; break;
            default:
                FAIL("select src port failure.");
            }
            switch (select_dst) {
            case select_port_reg: mov.dst = port_reg; break;
            case select_port_r_m: mov.dst = port_r_m; break;
            case select_port_mem: mov.dst = port_mem; break;
            default:
                FAIL("select dst port failure.");
            }
            fprint_mov_insn(fout, mov);
        } break;

        default:
            FAIL("Unknown opcode 0x%02x", a);
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
    abort();
}

int main(int argc, char **argv) {
    decode_stream(stdin, stdout, fatal_error);
    return 0;
}
