#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <libgen.h>   
#include <limits.h>   

#include "risc-hex-converter.h"

#define MAX_LINES 4096

static int reg_name_to_num(const char *name) {
    if (!name) return -1;

    if (name[0] == 'x') {
        int n = atoi(name + 1);
        if (n >= 0 && n < RISCV_REGS) return n;
    }

    if (strcmp(name, "zero") == 0) return 0;
    if (strcmp(name, "ra")   == 0) return 1;
    if (strcmp(name, "sp")   == 0) return 2;
    if (strcmp(name, "gp")   == 0) return 3;
    if (strcmp(name, "tp")   == 0) return 4;

    if (isdigit((unsigned char)name[0])) {
        int n = atoi(name);
        if (n >= 0 && n < RISCV_REGS) return n;
    }

    return -1;
}



enum OPCODE_TYPE get_opcode_type(uint32_t instruction) {
    uint8_t opcode = instruction & 0x7f;

    for (int i = 0; i < NUM_CODES; i++)
        if (opcodes[i].code == opcode)
            return opcodes[i].type;

    return ERROR;
}


static uint32_t encode_r(uint8_t rd, uint8_t rs1, uint8_t rs2, uint8_t funct3, uint8_t funct7, uint8_t opcode) {
    return ((uint32_t)funct7  << 25) |
           ((uint32_t)rs2     << 20) |
           ((uint32_t)rs1     << 15) |
           ((uint32_t)funct3  << 12) |
           ((uint32_t)rd      <<  7) |
           opcode;
}

static uint32_t encode_i(uint8_t rd, uint8_t rs1, int32_t imm, uint8_t funct3, uint8_t opcode) {
    uint32_t imm12 = (uint32_t)imm & 0xfff;

    return (imm12            << 20) |
           ((uint32_t)rs1    << 15) |
           ((uint32_t)funct3 << 12) |
           ((uint32_t)rd     <<  7) |
           opcode;
}

static uint32_t encode_s(uint8_t rs1, uint8_t rs2, int32_t imm, uint8_t funct3, uint8_t opcode) {
    uint32_t imm12   = (uint32_t)imm & 0xfff;
    uint8_t  imm_lo  =  imm12        & 0x1f;   
    uint8_t  imm_hi  = (imm12 >> 5)  & 0x7f;   

    return ((uint32_t)imm_hi  << 25) |
           ((uint32_t)rs2     << 20) |
           ((uint32_t)rs1     << 15) |
           ((uint32_t)funct3  << 12) |
           ((uint32_t)imm_lo  <<  7) |
           opcode;
}

static uint32_t encode_b(uint8_t rs1, uint8_t rs2, int32_t byte_offset, uint8_t funct3, uint8_t opcode) {
    uint8_t bit12    = (byte_offset >> 12) & 0x1;
    uint8_t bit11    = (byte_offset >> 11) & 0x1;
    uint8_t bits10_5 = (byte_offset >>  5) & 0x3f;
    uint8_t bits4_1  = (byte_offset >>  1) & 0xf;

    return ((uint32_t)bit12    << 31) |
           ((uint32_t)bits10_5 << 25) |
           ((uint32_t)rs2      << 20) |
           ((uint32_t)rs1      << 15) |
           ((uint32_t)funct3   << 12) |
           ((uint32_t)bits4_1  <<  8) |
           ((uint32_t)bit11    <<  7) |
           opcode;
}

static uint32_t encode_u(uint8_t rd, int32_t imm, uint8_t opcode) {
    uint32_t upper20 = (uint32_t)imm & 0xfffff000;

    return upper20 | ((uint32_t)rd << 7) | opcode;
}

static uint32_t encode_j(uint8_t rd, int32_t byte_offset, uint8_t opcode) {
    uint8_t  bit20     = (byte_offset >> 20) & 0x1;
    uint8_t  bit11     = (byte_offset >> 11) & 0x1;
    uint8_t  bits19_12 = (byte_offset >> 12) & 0xff;
    uint16_t bits10_1  = (byte_offset >>  1) & 0x3ff;

    return ((uint32_t)bit20     << 31) |
           ((uint32_t)bits19_12 << 12) |
           ((uint32_t)bit11     << 20) |
           ((uint32_t)bits10_1  << 21) |
           ((uint32_t)rd        <<  7) |
           opcode;
}


static void trim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    memmove(s, start, strlen(start) + 1);

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1)))
        *--end = '\0';
}

static int is_number(const char *s) {
    if (!s || !*s) return 0;

    const char *p = s;
    if (*p == '+' || *p == '-') p++;

    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        if (!*p) return 0;
        while (*p) {
            if (!isxdigit((unsigned char)*p)) return 0;
            p++;
        }
        return 1;
    }

    if (!isdigit((unsigned char)*p)) return 0;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return 0;
        p++;
    }
    return 1;
}

static int lookup_label(const char *name, char **label_names, int *label_addrs, int count) {
    for (int i = 0; i < count; i++)
        if (strcmp(name, label_names[i]) == 0)
            return label_addrs[i];
    return -1;
}

static uint32_t assemble_line(char *line, int line_idx, char **label_names, int *label_addrs, int num_labels) {
    char *tok[8];
    int   num_toks = 0;
    for (char *p = strtok(line, " \t,"); p && num_toks < 8; p = strtok(NULL, " \t,"))
        tok[num_toks++] = p;

    if (num_toks == 0) return 0;

    for (int i = 0; i < num_toks; i++) {
        size_t len = strlen(tok[i]);
        if (len > 0 && tok[i][len - 1] == ':')
            tok[i][len - 1] = '\0';
    }

    const char *instruct = tok[0];

    int has_imm_form =
        !strcmp(instruct, "add") || !strcmp(instruct, "sub") ||
        !strcmp(instruct, "sll") || !strcmp(instruct, "srl") ||
        !strcmp(instruct, "sra");

    int is_r_instruct =
        has_imm_form || !strcmp(instruct, "slt") || !strcmp(instruct, "sltu") ||
        !strcmp(instruct, "xor") || !strcmp(instruct, "or") || !strcmp(instruct, "and");

    if (has_imm_form && num_toks >= 4 && is_number(tok[3]) && reg_name_to_num(tok[3]) < 0) {
        uint8_t rd  = reg_name_to_num(tok[1]);
        uint8_t rs1 = reg_name_to_num(tok[2]);
        int32_t imm = strtol(tok[3], NULL, 0);
        uint8_t funct3;

        if (!strcmp(instruct, "add") || !strcmp(instruct, "sub")) 
			funct3 = ADD_SUB;
        else if (!strcmp(instruct, "sll"))                             
			funct3 = SLL;
        else if (!strcmp(instruct, "srl")) { 
			funct3 = SRL_SRA; 
			imm &= 0x1f;          
		}
        else { 
			funct3 = SRL_SRA; 
			imm  = (imm & 0x1f) | 0x20; 
		}

        return encode_i(rd, rs1, imm, funct3, 0b0010011);
    }

    if (is_r_instruct) {
        uint8_t rd = reg_name_to_num(tok[1]);
        uint8_t rs1 = reg_name_to_num(tok[2]);
        uint8_t rs2 = reg_name_to_num(tok[3]);
        uint8_t funct3, funct7;

        if (!strcmp(instruct, "add"))  { 
			funct3 = ADD_SUB; 
			funct7 = ADD; 
		}
        else if (!strcmp(instruct, "sub"))  { 
			funct3 = ADD_SUB; 
			funct7 = SUB; 
		}
        else if (!strcmp(instruct, "sll"))  { 
			funct3 = SLL;     
			funct7 = ADD; 
		}
        else if (!strcmp(instruct, "slt"))  { 
			funct3 = SLT;     
			funct7 = ADD; 
		}
        else if (!strcmp(instruct, "sltu")) { 
			funct3 = SLTU;    
			funct7 = ADD; 
		}
        else if (!strcmp(instruct, "xor"))  { 
			funct3 = XOR;     
			funct7 = ADD; 
		}
        else if (!strcmp(instruct, "srl"))  { 
			funct3 = SRL_SRA; 
			funct7 = SRL; 
		}
        else if (!strcmp(instruct, "sra"))  { 
			funct3 = SRL_SRA; 
			funct7 = SRA; 
		}
        else if (!strcmp(instruct, "or"))   { 
			funct3 = OR;      
			funct7 = ADD; 
		}
        else { 
			funct3 = AND;
			funct7 = ADD; 
		}

        return encode_r(rd, rs1, rs2, funct3, funct7, 0b0110011);
    }

    if (!strcmp(instruct, "addi")  || !strcmp(instruct, "slti")  ||
        !strcmp(instruct, "sltiu") || !strcmp(instruct, "xori")  ||
        !strcmp(instruct, "ori")   || !strcmp(instruct, "andi")  ||
        !strcmp(instruct, "slli")  || !strcmp(instruct, "srli")  ||
        !strcmp(instruct, "srai")  || !strcmp(instruct, "lw")) {
        uint8_t rd  = reg_name_to_num(tok[1]);
        uint8_t rs1;
        int32_t imm;

        if (!strcmp(instruct, "lw")) {
            char *paren = strchr(tok[2], '(');
            imm = strtol(tok[2], NULL, 0);
            rs1 = reg_name_to_num(paren + 1);
        } else {
            rs1 = reg_name_to_num(tok[2]);
            imm = strtol(tok[3], NULL, 0);
        }

        uint8_t funct3;
        if (!strcmp(instruct, "addi"))  funct3 = ADD_SUB;
        else if (!strcmp(instruct, "slti"))  funct3 = SLT;
        else if (!strcmp(instruct, "sltiu")) funct3 = SLTU;
        else if (!strcmp(instruct, "xori"))  funct3 = XOR;
        else if (!strcmp(instruct, "ori"))   funct3 = OR;
        else if (!strcmp(instruct, "andi"))  funct3 = AND;
        else if (!strcmp(instruct, "slli"))  funct3 = SLL;
        else if (!strcmp(instruct, "srli")) { 
			funct3 = SRL_SRA; imm &= 0x1f; 
		}
        else if (!strcmp(instruct, "srai")) { 
			funct3 = SRL_SRA; imm |= 0x20; 
		}
        else funct3 = 0x2;

        uint8_t opcode = strcmp(instruct, "lw") ? 0b0010011 : 0b0000011;
        return encode_i(rd, rs1, imm, funct3, opcode);
    }

    if (!strcmp(instruct, "beq") || !strcmp(instruct, "bne") ||
        !strcmp(instruct, "blt") || !strcmp(instruct, "bge"))
    {
        uint8_t rs1 = reg_name_to_num(tok[1]);
        uint8_t rs2 = reg_name_to_num(tok[2]);

        int target_idx = lookup_label(tok[3], label_names, label_addrs, num_labels);
        if (target_idx < 0) {
            printf("Error");
            return 0;
        }

        int32_t offset = (target_idx - line_idx) * 4;

        uint8_t funct3;
        if (!strcmp(instruct, "beq")) funct3 = 0x0;
        else if (!strcmp(instruct, "bne")) funct3 = 0x1;
        else if (!strcmp(instruct, "blt")) funct3 = 0x4;
        else funct3 = 0x5;

        return encode_b(rs1, rs2, offset, funct3, 0b1100011);
    }

    if (!strcmp(instruct, "j") || !strcmp(instruct, "jal")) {
        int is_jal = !strcmp(instruct, "jal");

        uint8_t rd = is_jal ? reg_name_to_num(tok[1]) : 0;
        const char *label_name = is_jal ? tok[2] : tok[1];

        int target_idx = lookup_label(label_name, label_names, label_addrs, num_labels);
        if (target_idx < 0) {
            printf("Error");
            return 0;
        }

        int32_t offset = (target_idx - line_idx) * 4;
        return encode_j(rd, offset, 0b1101111);
    }

    printf("Error");
    return 0;
}


void run(void) {
    printf("Running RISC-V hex converter...\n\n");

    FILE *fp_in = fopen(prog_file, "r");
    if (!fp_in) {
        fprintf(stderr, "Error: cannot open source file '%s'\n", prog_file);
        exit(EXIT_FAILURE);
    }

    char *lines[MAX_LINES];
    char *label_names[MAX_LINES];
    int   label_addrs[MAX_LINES];
    int   num_lines  = 0;
    int   num_labels = 0;

    char buf[256];
    while (fgets(buf, sizeof buf, fp_in)) {
        char *comment = strchr(buf, '#');
        if (comment) *comment = '\0';

        trim(buf);
        if (buf[0] == '\0') continue;

        char *colon = strchr(buf, ':');
        if (colon && *(colon + 1) == '\0' && strchr(buf, ' ') == NULL) {
            *colon = '\0';
            label_names[num_labels] = strdup(buf);
            label_addrs[num_labels] = num_lines;
            num_labels++;
            continue;
        }

        lines[num_lines++] = strdup(buf);
    }
    fclose(fp_in);

    char out_path[PATH_MAX];
    snprintf(out_path, sizeof out_path, "%s/../IO/output.txt",
             dirname(strdup(prog_file)));

    FILE *fp_out = fopen(out_path, "w");
    if (!fp_out) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", out_path);
        exit(EXIT_FAILURE);
    }

    PROGRAM_SIZE = 0;
    for (int i = 0; i < num_lines; i++) {
        printf("%s\n", lines[i]);

        uint32_t word = assemble_line(lines[i], i, label_names, label_addrs, num_labels);
        fprintf(fp_out, "%08x\n", word);
        PROGRAM_SIZE++;
        free(lines[i]);
    }

    fclose(fp_out);

    printf("\nDone — %u instruction(s) written to %s\n\n", PROGRAM_SIZE, out_path);

    for (int i = 0; i < num_labels; i++)
        free(label_names[i]);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source.asm>\n", argv[0]);
        return EXIT_FAILURE;
    }

    strncpy(prog_file, argv[1], sizeof prog_file - 1);
    prog_file[sizeof prog_file - 1] = '\0';

    run();
    return EXIT_SUCCESS;
}