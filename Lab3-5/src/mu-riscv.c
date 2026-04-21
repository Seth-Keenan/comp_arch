#include "mu-riscv.h"

/***************************************************************/
/* Print out a list of commands available                                                                  */
/***************************************************************/
void help() {
	printf("------------------------------------------------------------------\n\n");
	printf("\t**********MU-RISCV Help MENU**********\n\n");
	printf("sim\t-- simulate program to completion \n");
	printf("run <n>\t-- simulate program for <n> instructions\n");
	printf("rdump\t-- dump register values\n");
	printf("reset\t-- clears all registers/memory and re-loads the program\n");
	printf("input <reg> <val>\t-- set GPR <reg> to <val>\n");
	printf("mdump <start> <stop>\t-- dump memory from <start> to <stop> address\n");
	printf("high <val>\t-- set the HI register to <val>\n");
	printf("low <val>\t-- set the LO register to <val>\n");
	printf("print\t-- print the program loaded into memory\n");
	printf("show\t-- print the current content of the pipeline registers\n");
	printf("f [0 | 1]\t-- Enable/disable forwarding.\n");
	printf("?\t-- display help menu\n");
	printf("quit\t-- exit the simulator\n\n");
	printf("------------------------------------------------------------------\n\n");
}

/***************************************************************/
/* Read a 32-bit word from memory                                                                            */
/***************************************************************/
uint32_t mem_read_32(uint32_t address)
{
	int i;
	for (i = 0; i < NUM_MEM_REGION; i++) {
		if ( (address >= MEM_REGIONS[i].begin) &&  ( address <= MEM_REGIONS[i].end) ) {
			uint32_t offset = address - MEM_REGIONS[i].begin;
			return (MEM_REGIONS[i].mem[offset+3] << 24) |
					(MEM_REGIONS[i].mem[offset+2] << 16) |
					(MEM_REGIONS[i].mem[offset+1] <<  8) |
					(MEM_REGIONS[i].mem[offset+0] <<  0);
		}
	}
	return 0;
}

/***************************************************************/
/* Write a 32-bit word to memory                                                                                */
/***************************************************************/
void mem_write_32(uint32_t address, uint32_t value)
{
	int i;
	uint32_t offset;
	for (i = 0; i < NUM_MEM_REGION; i++) {
		if ( (address >= MEM_REGIONS[i].begin) && (address <= MEM_REGIONS[i].end) ) {
			offset = address - MEM_REGIONS[i].begin;

			MEM_REGIONS[i].mem[offset+3] = (value >> 24) & 0xFF;
			MEM_REGIONS[i].mem[offset+2] = (value >> 16) & 0xFF;
			MEM_REGIONS[i].mem[offset+1] = (value >>  8) & 0xFF;
			MEM_REGIONS[i].mem[offset+0] = (value >>  0) & 0xFF;
		}
	}
}

/***************************************************************/
/* Execute one cycle                                                                                                              */
/***************************************************************/
void cycle() {
	handle_pipeline();
	CURRENT_STATE = NEXT_STATE;
	CYCLE_COUNT++;
}

/***************************************************************/
/* Simulate RISCV for n cycles                                                                                       */
/***************************************************************/
void run(int num_cycles) {

	if (RUN_FLAG == FALSE) {
		printf("Simulation Stopped\n\n");
		return;
	}

	printf("Running simulator for %d cycles...\n\n", num_cycles);
	int i;
	for (i = 0; i < num_cycles; i++) {
		if (RUN_FLAG == FALSE) {
			printf("Simulation Stopped.\n\n");
			break;
		}
		printf("Cycle %d:", i + 1);
		cycle();
	}
}

/***************************************************************/
/* simulate to completion                                                                                               */
/***************************************************************/
void runAll() {
	if (RUN_FLAG == FALSE) {
		printf("Simulation Stopped.\n\n");
		return;
	}

	printf("Simulation Started...\n\n");
	while (RUN_FLAG){
		cycle();
	}
	printf("Simulation Finished.\n\n");
}

/***************************************************************/
/* Dump a word-aligned region of memory to the terminal                              */
/***************************************************************/
void mdump(uint32_t start, uint32_t stop) {
	uint32_t address;

	printf("-------------------------------------------------------------\n");
	printf("Memory content [0x%08x..0x%08x] :\n", start, stop);
	printf("-------------------------------------------------------------\n");
	printf("\t[Address in Hex (Dec) ]\t[Value]\n");
	for (address = start; address <= stop; address += 4){
		printf("\t0x%08x (%d) :\t0x%08x\n", address, address, mem_read_32(address));
	}
	printf("\n");
}

/***************************************************************/
/* Dump current values of registers to the teminal                                              */
/***************************************************************/
void rdump() {
	int i;
	printf("-------------------------------------\n");
	printf("Dumping Register Content\n");
	printf("-------------------------------------\n");
	printf("# Instructions Executed\t: %u\n", INSTRUCTION_COUNT);
	printf("PC\t: 0x%08x\n", CURRENT_STATE.PC);
	printf("-------------------------------------\n");
	printf("[Register]\t[Value]\n");
	printf("-------------------------------------\n");
	for (i = 0; i < RISCV_REGS; i++){
		printf("[R%d]\t: 0x%08x\n", i, CURRENT_STATE.REGS[i]);
	}
	printf("-------------------------------------\n");
	printf("[HI]\t: 0x%08x\n", CURRENT_STATE.HI);
	printf("[LO]\t: 0x%08x\n", CURRENT_STATE.LO);
	printf("-------------------------------------\n");
}

/***************************************************************/
/* Read a command from standard input.                                                               */
/***************************************************************/
void handle_command() {
	char buffer[20];
	uint32_t start, stop, cycles;
	uint32_t register_no;
	int register_value;
	int hi_reg_value, lo_reg_value;

	printf("MU-RISCV SIM:> ");

	if (scanf("%s", buffer) == EOF){
		exit(0);
	}

	switch(buffer[0]) {
		case 'S':
		case 's':
			if (buffer[1] == 'h' || buffer[1] == 'H'){
				show_pipeline();
			}else {
				runAll();
			}
			break;
		case 'M':
		case 'm':
			if (scanf("%x %x", &start, &stop) != 2){
				break;
			}
			mdump(start, stop);
			break;
		case '?':
			help();
			break;
		case 'Q':
		case 'q':
			printf("**************************\n");
			printf("Exiting MU-RISCV! Good Bye...\n");
			printf("**************************\n");
			exit(0);
		case 'R':
		case 'r':
			if (buffer[1] == 'd' || buffer[1] == 'D'){
				rdump();
			}else if(buffer[1] == 'e' || buffer[1] == 'E'){
				reset();
			}
			else {
				if (scanf("%d", &cycles) != 1) {
					break;
				}
				run(cycles);
			}
			break;
		case 'I':
		case 'i':
			if (scanf("%u %i", &register_no, &register_value) != 2){
				break;
			}
			CURRENT_STATE.REGS[register_no] = register_value;
			NEXT_STATE.REGS[register_no] = register_value;
			break;
		case 'H':
		case 'h':
			if (scanf("%i", &hi_reg_value) != 1){
				break;
			}
			CURRENT_STATE.HI = hi_reg_value;
			NEXT_STATE.HI = hi_reg_value;
			break;
		case 'L':
		case 'l':
			if (scanf("%i", &lo_reg_value) != 1){
				break;
			}
			CURRENT_STATE.LO = lo_reg_value;
			NEXT_STATE.LO = lo_reg_value;
			break;
		case 'P':
		case 'p':
			print_program();
			break;
		case 'F':
		case 'f':
			if(scanf("%d", &ENABLE_FORWARDING) != 1) {
				break;
			}
			else {
				ENABLE_FORWARDING == 0 ? printf("Forwarding OFF\n") : printf("Forwarding ON\n");
				break;
			}
		default:
			printf("Invalid Command.\n");
			break;
	}
}

/***************************************************************/
/* reset registers/memory and reload program                                                    */
/***************************************************************/
void reset() {
	int i;
	/*reset registers*/
	for (i = 0; i < RISCV_REGS; i++){
		CURRENT_STATE.REGS[i] = 0;
	}
	CURRENT_STATE.HI = 0;
	CURRENT_STATE.LO = 0;

	for (i = 0; i < NUM_MEM_REGION; i++) {
		uint32_t region_size = MEM_REGIONS[i].end - MEM_REGIONS[i].begin + 1;
		memset(MEM_REGIONS[i].mem, 0, region_size);
	}

	/*load program*/
	load_program();

	/*reset PC*/
	INSTRUCTION_COUNT = 0;
	CURRENT_STATE.PC =  MEM_TEXT_BEGIN;
	NEXT_STATE = CURRENT_STATE;
	RUN_FLAG = TRUE;
}

/***************************************************************/
/* Allocate and set memory to zero                                                                            */
/***************************************************************/
void init_memory() {
	int i;
	for (i = 0; i < NUM_MEM_REGION; i++) {
		uint32_t region_size = MEM_REGIONS[i].end - MEM_REGIONS[i].begin + 1;
		MEM_REGIONS[i].mem = malloc(region_size);
		memset(MEM_REGIONS[i].mem, 0, region_size);
	}
}

/**************************************************************/
/* load program into memory                                                                                      */
/**************************************************************/
void load_program() {
	FILE * fp;
	int i, word;
	uint32_t address;

	/* Open program file. */
	fp = fopen(prog_file, "r");
	if (fp == NULL) {
		printf("Error: Can't open program file %s\n", prog_file);
		exit(-1);
	}

	/* Read in the program. */

	i = 0;
	while( fscanf(fp, "%x\n", &word) != EOF ) {
		address = MEM_TEXT_BEGIN + i;
		mem_write_32(address, word);
		printf("writing 0x%08x into address 0x%08x (%d)\n", word, address, address);
		i += 4;
	}
	PROGRAM_SIZE = i/4;
	printf("Program loaded into memory.\n%d words written into memory.\n\n", PROGRAM_SIZE);
	fclose(fp);
}

/************************************************************/
/* maintain the pipeline                                                                                           */
/************************************************************/
void handle_pipeline()
{
	// Test show pipeline here
	show_pipeline();

	WB();
	MEM();
	EX();
	ID();
	if(!bubble) {
		IF();
	}
	bubble = false;
}

/************************************************************/
/* writeback (WB) pipeline stage:                                                                          */
/************************************************************/
void WB()
{
	uint32_t ir = MEM_WB.IR;
	if (ir == 0) return;

	uint8_t opcode = GET_OPCODE(ir);
	uint8_t rd = (ir >> 7) & BIT_MASK_5;

	switch (opcode) {
		case R_OPCODE:
			if (rd != 0) NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case IMM_ALU_OPCODE:
			if (rd != 0) NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case LOAD_OPCODE:
			if (rd != 0) NEXT_STATE.REGS[rd] = MEM_WB.LMD;
			break;
		default:
			break;
	}

	INSTRUCTION_COUNT++;
	if (INSTRUCTION_COUNT >= PROGRAM_SIZE) {
		RUN_FLAG = FALSE;
	}
}

/************************************************************/
/* memory access (MEM) pipeline stage:                                                          */
/************************************************************/
void MEM()
{
	MEM_WB.IR = EX_MEM.IR;

	if (EX_MEM.IR == 0) return;

	uint8_t opcode = GET_OPCODE(EX_MEM.IR);

	if (opcode == R_OPCODE || opcode == IMM_ALU_OPCODE)
	{
		MEM_WB.ALUOutput = EX_MEM.ALUOutput;
	}
	else if (opcode == LOAD_OPCODE) {
		MEM_WB.LMD = mem_read_32(EX_MEM.ALUOutput);
	} else if (opcode == STORE_OPCODE) {
		mem_write_32(EX_MEM.ALUOutput, EX_MEM.B);
	}
}
 

/************************************************************/
/* execution (EX) pipeline stage:                                                                          */
/************************************************************/
void EX()
{
	EX_MEM.IR = ID_EX.IR;
	uint32_t ir = ID_EX.IR;
	if (ir == 0) return;

	uint8_t opcode = GET_OPCODE(ir);
	uint8_t rs1 = (ir >> 15) & BIT_MASK_5;
	uint8_t rs2 = (ir >> 20) & BIT_MASK_5;

	uint32_t A = ID_EX.A;
	uint32_t B = ID_EX.B;

	branch_taken = false;

	if (ENABLE_FORWARDING) {

		// If A or be match rs1 or rs2 - 2 cycle
		if (MEM_WB.IR != 0) {
			uint8_t wb_op = GET_OPCODE(MEM_WB.IR);
			uint8_t wb_rd = 0;
			uint32_t wb_val = 0;
			if (wb_op == R_OPCODE || wb_op == IMM_ALU_OPCODE) {
				wb_rd  = (MEM_WB.IR >> 7) & BIT_MASK_5;
				wb_val = MEM_WB.ALUOutput;
			} else if (wb_op == LOAD_OPCODE) {
				wb_rd  = (MEM_WB.IR >> 7) & BIT_MASK_5;
				wb_val = MEM_WB.LMD;
			}
			if (wb_rd != 0) {
				if (wb_rd == rs1) A = wb_val;
				if (wb_rd == rs2) B = wb_val;
				printf("\n\nEX Forwarding Check MEM_WB\n\n");
			}
		}

		// If EX_MEM.IR matches registration register - 1 cycle
		if (EX_MEM.IR != 0) {
			uint8_t ex_op = GET_OPCODE(EX_MEM.IR);
			if (ex_op == R_OPCODE || ex_op == IMM_ALU_OPCODE) {
				uint8_t ex_rd = (EX_MEM.IR >> 7) & BIT_MASK_5;
				if (ex_rd != 0) {
					if (ex_rd == rs1) A = EX_MEM.ALUOutput;
					if (ex_rd == rs2) B = EX_MEM.ALUOutput;
					printf("\n\nEX Forwarding Check EX_MEM\n\n");
				}
			}
		}
	}

	uint8_t funct3 = ir >> 12 & BIT_MASK_3;
	uint8_t funct7 = ir >> 25 & BIT_MASK_7;
	if(opcode == IMM_ALU_OPCODE || opcode == R_OPCODE) {

	if (opcode == R_OPCODE) {
		switch (funct3) {
			case ADD_SUB:
				switch(funct7) {
					case ADD:
						EX_MEM.ALUOutput = A + B;
						break;
					case SUB:
						EX_MEM.ALUOutput = A - B;
						break;
					default:
						break;
				}
				break;
			case SLL:
				EX_MEM.ALUOutput = A << (B & 0x1F);
				break;
			case SLT:
				EX_MEM.ALUOutput = (int32_t)A < (int32_t)B ? 1 : 0;
				break;
			case SLTU:
				EX_MEM.ALUOutput = A < B ? 1 : 0;
				break;
			case XOR:
				EX_MEM.ALUOutput = A ^ B;
				break;
			case SRL_SRA:
				switch(funct7) {
					case SRL:
						EX_MEM.ALUOutput = A >> (B & 0x1F);
						break;
					case SRA:
						EX_MEM.ALUOutput = (int32_t)A >> (B & 0x1F);
						break;
					default:
						break;
				}
				break;
			case OR:
				EX_MEM.ALUOutput = A | B;
				break;
			case AND:
				EX_MEM.ALUOutput = A & B;
				break;
			default:
				break;
		}

		} else {
		EX_MEM.ALUOutput = A + ID_EX.imm;
		EX_MEM.B = B;
		}
	} else if(opcode == BRANCH_OPCODE) {
		uint8_t  f3 = funct3; 

		uint32_t imm_11 = (ir >> 7)  & 0x1;
		uint32_t imm_4_1 = (ir >> 8)  & 0xF;
		uint32_t imm_10_5 = (ir >> 25) & 0x3F;
		uint32_t imm_12 = (ir >> 31) & 0x1;

		int32_t imm = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);

		if (imm_12) imm |= 0xFFFFE000;

		if (f3 == 0) {          // BEQ
			branch_taken = (A == B);
		} else if (f3 == 1) {   // BNE
			branch_taken = (A != B);
		} else if (f3 == 4) {   // BLT
			branch_taken = ((int32_t)A < (int32_t)B);
		} else if (f3 == 5) {   // BGE
			branch_taken = ((int32_t)A >= (int32_t)B);
		} else if (f3 == 6) {   // BLTU
			branch_taken = (A < B);
		} else if (f3 == 7) {   // BGEU
			branch_taken = (A >= B);
		} else {
			printf("Invalid Branch Command!\n");
			return;
		}

		if (branch_taken) {
			// Flush last instruction
			IF_ID.IR  = 0;
			IF_ID.A   = 0;
			IF_ID.B   = 0;
			IF_ID.imm = 0;
			
			ID_EX.IR  = 0;
			ID_EX.A   = 0;
			ID_EX.B   = 0;
			ID_EX.imm = 0;
			bubble = true;

			IF_ID.PC = CURRENT_STATE.PC + imm;

			printf("\n\nSTALL - Branch Hazard (taken)\n\n");
		} else {
			// Not taken - increment pc counter happens in IF
			ID_EX.IR  = 0;
			ID_EX.A   = 0;
			ID_EX.B   = 0;
			ID_EX.imm = 0;
			bubble = true;
			printf("\n\nSTALL - Branch Hazard (not taken)\n\n");
		}
	} else if(opcode == JUMP_OPCODE) {		
		uint32_t rd = (ir >> 7) & BIT_MASK_5;
		uint32_t imm_19_12 = (ir >> 12) & 0xFF;
		uint32_t imm_11 = (ir >> 20) & 0x1;
		uint32_t imm_10_1 = (ir >> 21) & 0x3FF;
		uint32_t imm_20 = (ir >> 31) & 0x1;

		int32_t imm = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);

		if (imm_20) imm |= 0xFFE00000;

		branch_taken = true;

		NEXT_STATE.REGS[rd] = (uint32_t)(ID_EX.PC + 4);
		IF_ID.PC += CURRENT_STATE.PC + imm;

		IF_ID.IR  = 0;
		IF_ID.A   = 0;
		IF_ID.B   = 0;
		IF_ID.imm = 0;
		
		ID_EX.IR  = 0;
		ID_EX.A   = 0;
		ID_EX.B   = 0;
		ID_EX.imm = 0;

		bubble = true;

	} else if (opcode == JALR_OPCODE) {
		uint32_t rd = (ir >> 7) & BIT_MASK_5;
		uint32_t func3 = (ir >> 12) & BIT_MASK_3;
		uint32_t rs1 = (ir >> 15) & BIT_MASK_5;
		int32_t imm = (int32_t)(ir) >> 20;
		
		branch_taken = true;

		IF_ID.IR  = 0;
		IF_ID.A   = 0;
		IF_ID.B   = 0;
		IF_ID.imm = 0;
		
		ID_EX.IR  = 0;
		ID_EX.A   = 0;
		ID_EX.B   = 0;
		ID_EX.imm = 0;

		bubble = true;

		NEXT_STATE.REGS[rd] = (ID_EX.PC + 4);
		IF_ID.PC = (CURRENT_STATE.REGS[rs1] + imm);
		// IF_ID.PC = (CURRENT_STATE.REGS[rs1] + imm) & ~1;

	} else {
		EX_MEM.ALUOutput = A + ID_EX.imm;
		EX_MEM.B = B;
	} 
}

/************************************************************/
/* instruction decode (ID) pipeline stage:                                                         */
/************************************************************/
void ID()
{
	uint32_t ir = IF_ID.IR;

	// Load use hazard case
	if (ID_EX.IR != 0 && GET_OPCODE(ID_EX.IR) == LOAD_OPCODE) {
		uint8_t load_rd = (ID_EX.IR >> 7) & BIT_MASK_5;
		uint8_t rs1 = (ir >> 15) & BIT_MASK_5;
		uint8_t rs2 = (ir >> 20) & BIT_MASK_5;
		if (load_rd != 0 && (load_rd == rs1 || load_rd == rs2)) {
			ID_EX.IR  = 0;
			ID_EX.A   = 0;
			ID_EX.B   = 0;
			ID_EX.imm = 0;
			bubble = true;
			printf("\n\nSTALL - Load Hazard\n\n");
			return;
		}
	}

	uint8_t ie_rs1 = (ir >> 15) & BIT_MASK_5;
	uint8_t ie_rs2 = (ir >> 20) & BIT_MASK_5;
	if (GET_OPCODE(EX_MEM.IR) == R_OPCODE || GET_OPCODE(EX_MEM.IR) == IMM_ALU_OPCODE)
	{
		uint8_t em_rd = (EX_MEM.IR >> 7) & BIT_MASK_5;
		if (em_rd != 0 && (em_rd == ie_rs1 || em_rd == ie_rs2))
		{
			ID_EX.IR  = 0;
			ID_EX.A   = 0;
			ID_EX.B   = 0;
			ID_EX.imm = 0;
			bubble = true;
			printf("\n\nSTALL - 1 Cycle Ahead\n\n");
			return;
		}
	}
	else if (GET_OPCODE(MEM_WB.IR) == R_OPCODE || GET_OPCODE(MEM_WB.IR) == IMM_ALU_OPCODE)
	{
		uint8_t mw_rd = (MEM_WB.IR >> 7) & BIT_MASK_5;
		if (mw_rd != 0 && (mw_rd == ie_rs1 || mw_rd == ie_rs2))
		{
			ID_EX.IR  = 0;
			ID_EX.A   = 0;
			ID_EX.B   = 0;
			ID_EX.imm = 0;
			bubble = true;
			printf("\n\nSTALL - 2 Cycles Ahead\n\n");
			return;
		}
	}

	ID_EX.IR = ir;
	ID_EX.A = CURRENT_STATE.REGS[(ir >> 15) & BIT_MASK_5];
	ID_EX.B = CURRENT_STATE.REGS[(ir >> 20) & BIT_MASK_5];

	uint8_t opcode = GET_OPCODE(ir);
	if(opcode == IMM_ALU_OPCODE || opcode == R_OPCODE) {
		ID_EX.imm = ir >> 20;
	} else if (opcode == STORE_OPCODE) {
		int32_t imm = (((int32_t)(ir) >> 25) << 5) | ((ir >> 7) & 0x1F);
		ID_EX.imm = (imm << 20) >> 20;
	} else {
		ID_EX.imm = 0;
	}
}


/************************************************************/
/* instruction fetch (IF) pipeline stage:                                                              */
/************************************************************/
void IF()
{
	uint32_t pc = CURRENT_STATE.PC;
	if (pc >= MEM_TEXT_BEGIN && pc < MEM_TEXT_BEGIN + PROGRAM_SIZE * 4) {
		IF_ID.IR = mem_read_32(pc);
	} else {
		IF_ID.IR = 0;
	}

	if (!branch_taken) {
		IF_ID.PC = pc + 4;
		NEXT_STATE.PC = pc + 4;
	}
}

/************************************************************/
/* Initialize Memory                                                                                                    */
/************************************************************/
void initialize() {
	init_memory();
	CURRENT_STATE.PC = MEM_TEXT_BEGIN;
	NEXT_STATE = CURRENT_STATE;
	RUN_FLAG = TRUE;
}

/************************************************************/
/* Print the program loaded into memory (in RISCV assembly format)    */
/************************************************************/
void print_program() {
	for(uint32_t mem_tracer = MEM_TEXT_BEGIN; 
		mem_tracer < MEM_TEXT_BEGIN + PROGRAM_SIZE*4; 
		mem_tracer+=4) {
		uint32_t cmd = mem_read_32(mem_tracer);
		print_command(cmd);
		printf("\n");
	}
}

/************************************************************/
/* Print the instruction at given memory address (in RISCV assembly format)    */
/************************************************************/
void print_command(uint32_t bincmd) {
	if(bincmd) {
		uint8_t opcode = GET_OPCODE(bincmd);
		switch (opcode) {
			case R_OPCODE:
				handle_r_print(bincmd);
				break;
			case STORE_OPCODE:
				handle_s_print(bincmd);
				break;
			case IMM_ALU_OPCODE:
				handle_i_print(bincmd);
				break;
			case LOAD_OPCODE:
				handle_i_print(bincmd);
				break;
			case BRANCH_OPCODE:
				handle_b_print(bincmd);
				break;
			case JUMP_OPCODE:
				handle_j_print(bincmd);
				break;
			default:
				printf("Unknown command!");
				break;
		}
	}
}

void handle_r_print(uint32_t bincmd) {
	uint8_t rd = bincmd >> 7 & BIT_MASK_5;
	uint8_t funct3 = bincmd >> 12 & BIT_MASK_3;
	uint8_t rs1 = bincmd >> 15 & BIT_MASK_5;
	uint8_t rs2 = bincmd >> 20 & BIT_MASK_5;
	uint8_t funct7 = bincmd >> 25 & BIT_MASK_7;
	switch(funct3) {
		case 0x0:
			switch(funct7){
				case 0x0:
					print_r_cmd("add", rd, rs1, rs2);
					break;
				case 0x20:
					print_r_cmd("sub", rd, rs1, rs2);
					break;
				default:
					printf("No funct7(%d) for funct3(%d) found for R-type.", funct7, funct3);
					break;
			}
			break;
		case 0x1:
			print_r_cmd("sll", rd, rs1, rs2);
			break;
		case 0x2:
			print_r_cmd("slt", rd, rs1, rs2);
			break;
		case 0x3:
			print_r_cmd("sltu", rd, rs1, rs2);
			break;
		case 0x4:
			print_r_cmd("xor", rd, rs1, rs2);
			break;
		case 0x5:
			switch(funct7){
				case 0x0:
					print_r_cmd("srl", rd, rs1, rs2);
					break;
				case 0x20:
					print_r_cmd("sra", rd, rs1, rs2);
					break;
				default:
					printf("No funct7(%d) for funct3(%d) found for R-type.", funct7, funct3);
					break;
			}
			break;
		case 0x6:
			print_r_cmd("or", rd, rs1, rs2);
			break;
		case 0x7:
			print_r_cmd("and", rd, rs1, rs2);
			break;
		default:
			printf("Unknown funct3(%d) in R-type", funct3);
			break;
	}
}

void handle_s_print(uint32_t bincmd) {
	uint8_t imm4 = bincmd >> 7 & BIT_MASK_5;
	uint8_t f3 = bincmd >> 12 & BIT_MASK_3;
	uint8_t rs1 = bincmd >> 15 & BIT_MASK_5;
	uint8_t rs2 = bincmd >> 20 & BIT_MASK_5;
	uint8_t imm11 = bincmd >> 25 & BIT_MASK_7;
	uint16_t imm = (imm11 | imm4);
	switch(f3) {
		case 0x0:
			print_s_cmd("sb", rs2, imm, rs1);
			break;
		case 0x1:
			print_s_cmd("sh", rs2, imm, rs1);
			break;
		case 0x2:
			print_s_cmd("sw", rs2, imm, rs1);
			break;
		default:
			printf("Unknown funct3(%d) in S type", f3);
			break;
	}
}

void handle_i_print(uint32_t bincmd) {

	uint8_t opcode = GET_OPCODE(bincmd);
	uint8_t rd = bincmd >> 7 & BIT_MASK_5;
	uint8_t funct3 = bincmd >> 12 & BIT_MASK_3;
	uint8_t rs1 = bincmd >> 15 & BIT_MASK_5;
	uint16_t imm = bincmd >> 20 & (BIT_MASK_12);
	switch(opcode) {
		case IMM_ALU_OPCODE:
			switch(funct3) {
				case 0x0: 
					print_i_type1_cmd("addi", rd, rs1, imm);
					break;
				case 0x1:
					print_i_type1_cmd("slli", rd, rs1, imm);
					break;
				case 0x2:
					print_i_type1_cmd("slti", rd, rs1, imm);
					break;
				case 0x3:
					print_i_type1_cmd("sltiu", rd, rs1, imm);
					break;
				case 0x4:
					print_i_type1_cmd("xori", rd, rs1, imm);
					break;
				case 0x5:
					;
					uint8_t imm5 = imm >> 5;
					switch(imm5){
						case 0:
							print_i_type1_cmd("srli", rd, rs1, imm);
							break;
						case 0x20:
							print_i_type1_cmd("srai", rd, rs1, imm);
							break;
						default:
							printf("Invalid imm[11:5](%d) for I-Type opcode(%d) funct3(%d)", imm5, opcode, funct3);
							break;
					}
					break;
				case 0x6:
					print_i_type1_cmd("ori", rd, rs1, imm);
					break;
				case 0x7:
					print_i_type1_cmd("andi", rd, rs1, imm);
					break;
				default:
					printf("Invalid funct3(%d) for I-type opcode(%d)", funct3, opcode);
					break;
			}
			break;
		case LOAD_OPCODE:
			switch(funct3){
				case 0x0:
					print_i_type2_cmd("lb", rd, rs1, imm);
					break;
				case 0x1:
					print_i_type2_cmd("lh", rd, rs1, imm);
					break;
				case 0x2:
					print_i_type2_cmd("lw", rd, rs1, imm);
					break;
				case 0x4:
					print_i_type2_cmd("lbu", rd, rs1, imm);
					break;
				case 0x5:
					print_i_type2_cmd("lhu", rd, rs1, imm);
					break;
				default:
					printf("Unknown funct3(%d) for I-type opcode(%d).", funct3, opcode);
					break;
			}
			break;
		default:
			printf("Unknown opcode(%d) for I-Type.", opcode);
			break;
	}

}

void handle_b_print(uint32_t bincmd) {
	// b type all gross...
	uint16_t scrambled_imm =(((bincmd >> 25) & BIT_MASK_7) << 5) | ((bincmd >> 7) & BIT_MASK_5);
	uint8_t bit12 = (scrambled_imm >> 12) & 0b1;
	uint8_t bit11 = scrambled_imm & 0b1;
	uint8_t bits10to5 = (scrambled_imm >> 5) & 0b111111;
	uint8_t bits4to1 = (scrambled_imm >> 1) & 0b1111;

	uint16_t imm = ((bit12 << 12) | (bit11 << 11) | (bits10to5 << 5) | (bits4to1 << 1)) >> 1; // >> 1 because it shifts left 1 in addressing to ensure even
	uint8_t f3 = bincmd >> 12 & BIT_MASK_3;
	uint8_t rs1 = bincmd >> 15 & BIT_MASK_5;
	uint8_t rs2 = bincmd >> 20 & BIT_MASK_5;

	switch(f3) {
		case 0x0:
			print_b_cmd("beq", rs1, rs2, imm);
			break;
		case 0x1:
			print_b_cmd("beq", rs1, rs2, imm);
			break;
		case 0x4:
			print_b_cmd("blt", rs1, rs2, imm);
			break;
		case 0x5:
			print_b_cmd("bge", rs1, rs2, imm);
			break;
		case 0x6:
			print_b_cmd("bltu", rs1, rs2, imm);
			break;
		case 0x7:
			print_b_cmd("bgeu", rs1, rs2, imm);
			break;
		default:
			printf("Unknown funct3(%d) in B type", f3);
			break;
	}

}

void handle_j_print(uint32_t bincmd) {
	uint8_t rd = (bincmd >> 7) & BIT_MASK_5;
	uint16_t scrambled_imm = (bincmd >> 12) & BIT_MASK_20;
	uint8_t bit20 = (scrambled_imm >> 20) & 0b1;
	uint16_t bits10to1 = (scrambled_imm >> 9) & 0b1111111111;
	uint8_t bit11 = (scrambled_imm >> 8) & 0b1; 
	uint8_t bits19to12 = (scrambled_imm) & 0b11111111;
	uint16_t offset = ((bit20 << 20) | (bits19to12 << 12) | (bit11 << 11) | (bits10to1 << 1)) >> 1;
	printf("jal x%d, %d", rd, offset);
}

void print_r_cmd(char* cmd_name, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    printf("%s x%d, x%d, x%d", cmd_name, rd, rs1, rs2);
}

void print_s_cmd(char* cmd_name, uint8_t rs2, uint8_t offset, uint8_t rs1) {
    printf("%s x%d, %d(x%d)", cmd_name, rs2, offset, rs1);
}

void print_i_type1_cmd(char* cmd_name, uint8_t rd, uint8_t rs1, uint16_t imm) {
    printf("%s x%d, x%d, %d", cmd_name, rd, rs1, imm);
}

void print_i_type2_cmd(char* cmd_name, uint8_t rd, uint8_t rs1, uint16_t imm) {
    printf("%s x%d, %d(x%d)", cmd_name, rd, imm, rs1);
}

void print_b_cmd(char* cmd_name, uint8_t rs1, uint8_t rs2, uint16_t imm) {
	printf("%s x%d, x%d, %d", cmd_name, rs1, rs2, imm);
}

void show_pipeline()
{
	printf("\nShow Pipeline: \n");
	printf("Current PC: 0x%08x\n", CURRENT_STATE.PC);
	printf("IF/ID IR 0x%08x", IF_ID.IR);
	if (IF_ID.IR) {
		printf(" ");
		print_command(IF_ID.IR);
	} 
	printf("\n");
	printf("IF/ID PC 0x%08x\n", IF_ID.PC);
	printf("\n");
	printf("ID/EX IR 0x%08x", ID_EX.IR);
	if (ID_EX.IR) {
		printf(" ");
		print_command(ID_EX.IR);
	} 
	printf("\n");
	printf("ID/EX A 0x%08x\n", ID_EX.A);
	printf("ID/EX B 0x%08x\n", ID_EX.B);
	printf("ID/EX imm 0x%08x\n", ID_EX.imm);
	printf("\n");
	printf("EX/MEM IR 0x%08x", EX_MEM.IR);
	if (EX_MEM.IR) {
		printf(" ");
		print_command(EX_MEM.IR);
	}
	printf("\n");
	printf("EX/MEM A 0x%08x\n", EX_MEM.A);
	printf("EX/MEM B 0x%08x\n", EX_MEM.B);
	printf("EX/MEM ALUOutput 0x%08x\n", EX_MEM.ALUOutput);
	printf("\n");
	printf("MEM/WB IR 0x%08x", MEM_WB.IR);
	if (MEM_WB.IR) {
		printf(" ");
		print_command(MEM_WB.IR);
	} 
	printf("\n");
	printf("MEM/WB ALUOutput 0x%08x\n", MEM_WB.ALUOutput);
	printf("MEM/WB LMD 0x%08x\n\n", MEM_WB.LMD);
}

/***************************************************************/
/* main                                                                                                                                   */
/***************************************************************/
int main(int argc, char *argv[]) {
	printf("\n**************************\n");
	printf("Welcome to MU-RISCV SIM...\n");
	printf("**************************\n\n");

	if (argc < 2) {
		printf("Error: You should provide input file.\nUsage: %s <input program> \n\n",  argv[0]);
		exit(1);
	}

	strcpy(prog_file, argv[1]);
	initialize();
	load_program();
	help();
	while (1){
		handle_command();
	}
	return 0;
}
