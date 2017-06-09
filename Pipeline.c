#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUMMEMORY 16 /* Maximum number of data words in memory */
#define NUMREGS 8    /* Number of registers */

/* Opcode values for instructions */
#define R 0   //R format
#define LW 35 //I 
#define SW 43 //I
#define BEQ 4  //I  
#define HALT 63
#define ZERO 0 //Zero register

/* Funct values for R-type instructions */
#define ADD 32
#define SUB 34
#define AND 36

#define NOP 0

int inst_index = 0;

typedef struct IFIDStruct {
  unsigned int instr;              /* Integer representation of instruction */
  int PCPlus4;                     /* PC + 4 */
} IFIDType;

typedef struct IDEXStruct {
  unsigned int instr;              /* Integer representation of instruction */
  int PCPlus4;                     /* PC + 4 */
  int readData1;                   /* Contents of rs register */
  int readData2;                   /* Contents of rt register */
  int immed;                       /* Immediate field */
  int rsReg;                       /* Number of rs register */
  int rtReg;                       /* Number of rt register */
  int rdReg;                       /* Number of rd register */
  int branchTarget;                /* Branch target, obtained from immediate field */

} IDEXType;

typedef struct EXMEMStruct {
  unsigned int instr;              /* Integer representation of instruction */
  int aluResult;                   /* Result of ALU operation */
  int writeDataReg;                /* Contents of the rt register, used for store word */
  int writeReg;                    /* The destination register */ //which will be written by an LW
  int forwardRs;
  int forwardRt;
  int memLoadAddress;
  int dataMemAddress;
} EXMEMType;

typedef struct MEMWBStruct {
  unsigned int instr;              /* Integer representation of instruction */
  int writeDataMem;                /* Data read from memory */
  int writeDataALU;                /* Result from ALU operation */
  int writeReg;                    /* The destination register */  //which will be written by an LW in WB stage!
} MEMWBType;

//a full state holding all 'pipeline registers': component per stage
typedef struct stateStruct {
  int PC;                                 /* Program Counter */
  unsigned int instrMem[NUMMEMORY];       /* Instruction memory */
  int dataMem[NUMMEMORY];                 /* Data memory */
  int regFile[NUMREGS];                   /* Register file */
  IFIDType IFID;                          /* Current IFID pipeline register */
  IDEXType IDEX;                          /* Current IDEX pipeline register */
  EXMEMType EXMEM;                        /* Current EXMEM pipeline register */
  MEMWBType MEMWB;                        /* Current MEMWB pipeline register */
  int cycles;                             /* Number of cycles executed so far */

int stallCount;
} stateType;

int beginPipeline();
void printState(stateType*);
void initState(stateType*);
unsigned int instrToInt(char*, char*);
int get_opcode(unsigned int);
void printInstruction(unsigned int);
int isNop(unsigned int);
int IF(stateType *,stateType *);
int ID(stateType *,stateType *);
int EX(stateType *,stateType *);
int MEM(stateType *,stateType *);
int WB(stateType *,stateType *);

//int getBpbIndex();
//int getBranchPrediction();
//int updateBranchPrediction();

int main(){
    beginPipeline();
    return(0); 
}

int isNop(unsigned int instr){
int tmpop=-1,tmpfunct=-1;

if ((tmpop=get_opcode(instr)==NOP) && ((tmpfunct=get_funct(instr))==NOP))
	return 1;
return 0;
}

int IF(stateType *state,stateType *newState){
	newState->PC = state->PC + 4;
	newState->IFID.PCPlus4 = newState->PC;
	newState->IFID.instr = state->instrMem[(state->PC)/4];

return 0;
}

int ID(stateType *state,stateType *newState){
	newState->IDEX.instr = state->IFID.instr;
	newState->IDEX.PCPlus4 = state->IFID.PCPlus4;
	if (isNop(newState->IDEX.instr))
	{return 0;}
	newState->IDEX.rsReg = get_rs(newState->IDEX.instr);
	newState->IDEX.readData1 = state->regFile[newState->IDEX.rsReg];
	newState->IDEX.rdReg = get_rd(newState->IDEX.instr);
	newState->IDEX.rtReg = get_rt(newState->IDEX.instr);
	newState->IDEX.readData2 = state->regFile[newState->IDEX.rtReg];
	newState->IDEX.immed = get_immed(newState->IDEX.instr);
	
	if (get_opcode(newState->IDEX.instr) == 4)
	{
		newState->IDEX.branchTarget = newState->IDEX.immed;

		newState->PC = state->PC;
		newState->IFID.instr = 0;
		newState->stallCount = state->stallCount + 1;
	}
return 0;
}

int EX(stateType *state,stateType *newState){
	newState->EXMEM.forwardRs = 0;
	newState->EXMEM.forwardRt = 0;
	newState->EXMEM.instr = state->IDEX.instr;
	if (isNop(newState->EXMEM.instr))
	{return 0;}
	newState->EXMEM.writeDataReg = state->IDEX.rtReg;
	if (get_opcode(newState->EXMEM.instr) == 0)
	{
		newState->EXMEM.writeReg = state->IDEX.rdReg;
	}
	else
	{
		newState->EXMEM.writeReg = state->IDEX.rtReg;
	}
	newState->EXMEM.memLoadAddress = state->IDEX.rsReg + state->IDEX.immed;
	newState->EXMEM.dataMemAddress = state->IDEX.rsReg + state->IDEX.immed;
	
	if (get_opcode(newState->EXMEM.instr) == 4)
	{
		newState->EXMEM.aluResult = state->IDEX.readData1 - state->IDEX.readData2;
		if (newState->EXMEM.aluResult == 0)
		{
			newState->IFID.instr = state->instrMem[(state->IDEX.branchTarget)/4];
			newState->PC = state->IDEX.branchTarget + 4;
			newState->IFID.PCPlus4 = newState->PC + 4;
			newState->IDEX.instr = 0;
		}		
	}
	
	if (get_funct(newState->EXMEM.instr) == 32)
	{			
		newState->EXMEM.aluResult = state->IDEX.readData1 + state->IDEX.readData2;
	}
	else if (get_funct(newState->EXMEM.instr) == 34)
	{
		newState->EXMEM.aluResult = state->IDEX.readData1 - state->IDEX.readData2;
	}
	else if (get_funct(newState->EXMEM.instr) == 36)
	{
		newState->EXMEM.aluResult = state->IDEX.readData1 & state->IDEX.readData2;
	}
	else if (get_funct(newState->EXMEM.instr) == 36)
	{
		newState->EXMEM.aluResult = state->IDEX.readData1;
	}
	else if (get_opcode(newState->EXMEM.instr) == 38)
	{
		newState->EXMEM.aluResult = state->regFile[state->IDEX.rdReg / 4] ^ 0xffffffff;
	}
	
	//data hazards with lw (stall & note to MEM/WB to forward)
	if (get_opcode(state->IDEX.instr) == 35)
	{
		if (get_rs(state->IFID.instr) == get_rt(state->IDEX.instr) 
			|| (get_opcode(state->IFID.instr) != 35 && get_rt(state->IFID.instr) == get_rt(state->IDEX.instr)))
		{
			newState->IFID.instr = state->instrMem[(state->PC-4)/4];
			newState->PC = state->PC;
			newState->IDEX.instr = 0;
			newState->stallCount = state->stallCount + 1;
			if (get_rs(state->IFID.instr) == get_rt(state->IDEX.instr))
			{
				newState->EXMEM.forwardRs = 1;
			}
			if (get_opcode(state->IFID.instr) != 35 && get_rt(state->IFID.instr) == get_rt(state->IDEX.instr))
			{
				newState->EXMEM.forwardRt = 1;
			}
		}
	}
	//all other data hazards (forward from EX/MEM)
	else
	{
		if (get_rt(newState->IDEX.instr) == get_rd(newState->EXMEM.instr)) 
		{
			newState->IDEX.readData2 = newState->EXMEM.aluResult;
		}	
		if (get_rs(newState->IDEX.instr) == get_rd(newState->EXMEM.instr))
		{
			newState->IDEX.readData1 = newState->EXMEM.aluResult;
		}
	}
	
return 0;
}

int MEM(stateType *state,stateType *newState){
	newState->MEMWB.instr = state->EXMEM.instr;
	if (isNop(newState->MEMWB.instr))
	{return 0;}
	newState->MEMWB.writeReg = state->EXMEM.writeReg;
	newState->MEMWB.writeDataALU = state->EXMEM.aluResult;
	
	if (get_opcode(newState->MEMWB.instr) == 35)
	{
		newState->MEMWB.writeDataMem = state->dataMem[(state->EXMEM.memLoadAddress)/4];
		if (state->EXMEM.forwardRs == 1)
		{
			newState->IDEX.readData1 = newState->MEMWB.writeDataMem;
		}
		if (state->EXMEM.forwardRt == 1)
		{
			newState->IDEX.readData2 = newState->MEMWB.writeDataMem;
		}
	}
	
	if (get_opcode(newState->MEMWB.instr) == 43)
	{
		newState->dataMem[state->EXMEM.dataMemAddress/4] = state->regFile[get_rt(newState->MEMWB.instr)];
	}
	
	if (get_opcode(newState->MEMWB.instr) == 0)
	{
		if (get_rt(newState->IDEX.instr) == get_rd(state->EXMEM.instr)) 
		{
			newState->IDEX.readData2 = state->EXMEM.aluResult;
		}	
		if (get_rs(newState->IDEX.instr) == get_rd(state->EXMEM.instr))
		{
			newState->IDEX.readData1 = state->EXMEM.aluResult;
		}
	}
	
return 0;
}

int WB(stateType *state,stateType *newState){
	if (isNop(state->MEMWB.instr))
	{return 0;}
	
	if (get_opcode(state->MEMWB.instr) == 0)
	{
		newState->regFile[state->MEMWB.writeReg] = state->MEMWB.writeDataALU;
	}
	else if (get_opcode(state->MEMWB.instr) == 35)
	{
		newState->regFile[state->MEMWB.writeReg] = state->MEMWB.writeDataMem;
		if (get_rs(state->IFID.instr) == get_rt(state->MEMWB.instr))
		{
			newState->IDEX.readData1 = newState->MEMWB.writeDataMem;
		}
		if (get_opcode(state->IFID.instr) != 35 && get_rt(state->IFID.instr) == get_rt(state->MEMWB.instr))
		{
			newState->IDEX.readData2 = newState->MEMWB.writeDataMem;
		}
	}
	
return 0;
}

int beginPipeline(){ 

stateType state;           /* Contains the state of the entire pipeline before the cycle executes */ 
stateType newState;        /* Contains the state of the entire pipeline after the cycle executes */

initState(&state);         /* Initialize the state of the pipeline */

    
    while (1) { //main pipeline loop
        
        //1 loop iteration per cycle/////////////

	//print pc, data[0-15],regfile[0-7],useful pipeline state
        printState(&state); 

	/* If a halt instruction enters WB, Print statistics and exit */
        if (get_opcode(state.MEMWB.instr) == HALT) {
            printf("Total number of cycles executed: %d\n", state.cycles);
            // print the number of stalls, branches, and (?)mispredictions
            return 1;
            }
	//Before the tasks of the cycle, copy the current state into newState
	// to be modified in order to reflect all work done in this cycle
        newState = state;     
        newState.cycles++;

	//Modify newState to reflect state of pipeline after current cycle 
        //after cycle, state passes to the next stage, freeing up that stage component

        /* --------------------- IF stage --------------------- */
        
        IF(&state, &newState);

        /* --------------------- ID stage --------------------- */    
		
		ID(&state, &newState);


        /* --------------------- EX stage --------------------- */
		
		EX(&state, &newState);


        /* --------------------- MEM stage --------------------- */
		
		MEM(&state, &newState);


        /* --------------------- WB stage --------------------- */
		
		WB(&state, &newState);

        //The modified newState becomes the current 
        // state at the start of the next cycle 
        state = newState;    
    }
return 0;
}

/******************************************************************/
/* The initState function accepts a pointer to the current        */ 
/* state as an argument, initializing the state to pre-execution  */
/* state. In particular, all registers are zero'd out. All        */
/* instructions in the pipeline are NOOPS. Data and instruction   */
/* memory are initialized with the contents of the assembly       */
/* input file.                                                    */
/*****************************************************************/
void initState(stateType *statePtr){
    unsigned int dec_inst;
    int data_index = 0;
    //int inst_index = 0;
    char line[130];
    char instr[5];
    char args[130];
    char* arg; 

    statePtr->PC = 0;
    statePtr->cycles = 0;

    statePtr->stallCount = 0;

    // Zero out data, instructions, registers
    memset(statePtr->dataMem, 0, 4*NUMMEMORY);
    memset(statePtr->instrMem, 0, 4*NUMMEMORY);
    memset(statePtr->regFile, 0, 4*NUMREGS);

    /* Parse assembly file and initialize data/instruction memory */
    while(fgets(line, 130, stdin)){
        if(sscanf(line, "\t.%s %s", instr, args) == 2){
            arg = strtok(args, ",");
            while(arg != NULL){
                statePtr->dataMem[data_index] = atoi(arg);
                data_index += 1;
                arg = strtok(NULL, ","); 
            }  
        }
        else if(sscanf(line, "\t%s %s", instr, args) == 2){
            dec_inst = instrToInt(instr, args);
            statePtr->instrMem[inst_index] = dec_inst;
            inst_index += 1;
        }
    } 

    /* Zero-out all registers in pipeline to start */
    statePtr->IFID.instr = 0;
    statePtr->IFID.PCPlus4 = 0;

    statePtr->IDEX.instr = 0;
    statePtr->IDEX.PCPlus4 = 0;
    statePtr->IDEX.branchTarget = 0;
    statePtr->IDEX.readData1 = 0;
    statePtr->IDEX.readData2 = 0;
    statePtr->IDEX.immed = 0;
    statePtr->IDEX.rsReg = 0;
    statePtr->IDEX.rtReg = 0;
    statePtr->IDEX.rdReg = 0;
 
    statePtr->EXMEM.instr = 0;
    statePtr->EXMEM.aluResult = 0;
    statePtr->EXMEM.writeDataReg = 0;
    statePtr->EXMEM.writeReg = 0;

    statePtr->MEMWB.instr = 0;
    statePtr->MEMWB.writeDataMem = 0;
    statePtr->MEMWB.writeDataALU = 0;
    statePtr->MEMWB.writeReg = 0;
 }


/***************************************************************************************/
/*              You do not need to modify the functions below.                         */
/*                They are provided for your convenience.                              */
/***************************************************************************************/


/*************************************************************/
/* The printState function accepts a pointer to a state as   */
/* an argument and prints the formatted contents of          */
/* pipeline register.                                        */
/*************************************************************/
void printState(stateType *statePtr){
    int i;
    printf("\n********************\nState at the beginning of cycle %d:\n", statePtr->cycles+1);
    printf("\tPC = %d\n", statePtr->PC);
    printf("\tData Memory:\n");

    for (i=0; i<(NUMMEMORY/2); i++){
        printf("\t\tdataMem[%d] = %d\t\tdataMem[%d] = %d\n", 
            i, statePtr->dataMem[i], i+(NUMMEMORY/2), statePtr->dataMem[i+(NUMMEMORY/2)]);
    	}

    printf("\tRegisters:\n");
    for (i=0; i<(NUMREGS/2); i++){
        printf("\t\tregFile[%d] = %d\t\tregFile[%d] = %d\n", 
            i, statePtr->regFile[i], i+(NUMREGS/2), statePtr->regFile[i+(NUMREGS/2)]);
    	}

    printf("\tIF/ID:\n");
    printf("\t\tInstruction: ");
    printInstruction(statePtr->IFID.instr);
    printf("\t\tPCPlus4: %d\n", statePtr->IFID.PCPlus4);

    printf("\tID/EX:\n");
    printf("\t\tInstruction: ");
    printInstruction(statePtr->IDEX.instr);
    printf("\t\tPCPlus4: %d\n", statePtr->IDEX.PCPlus4);
    printf("\t\tbranchTarget: %d\n", statePtr->IDEX.branchTarget);
    printf("\t\treadData1: %d\n", statePtr->IDEX.readData1);
    printf("\t\treadData2: %d\n", statePtr->IDEX.readData2);
    printf("\t\timmed: %d\n", statePtr->IDEX.immed);
    printf("\t\trs: %d\n", statePtr->IDEX.rsReg);
    printf("\t\trt: %d\n", statePtr->IDEX.rtReg);
    printf("\t\trd: %d\n", statePtr->IDEX.rdReg);

    printf("\tEX/MEM:\n");
    printf("\t\tInstruction: ");
    printInstruction(statePtr->EXMEM.instr);
    printf("\t\taluResult: %d\n", statePtr->EXMEM.aluResult);
    printf("\t\twriteDataReg: %d\n", statePtr->EXMEM.writeDataReg);
    printf("\t\twriteReg:%d\n", statePtr->EXMEM.writeReg);

    printf("\tMEM/WB:\n");
    printf("\t\tInstruction: ");
    printInstruction(statePtr->MEMWB.instr);
    printf("\t\twriteDataMem: %d\n", statePtr->MEMWB.writeDataMem);
    printf("\t\twriteDataALU: %d\n", statePtr->MEMWB.writeDataALU);
    printf("\t\twriteReg: %d\n", statePtr->MEMWB.writeReg);

	printf("stalls: %d\n",statePtr->stallCount);
}

/*************************************************************/
/*  The instrToInt function converts an instruction from the */
/*  assembly file into an unsigned integer representation.   */
/*  For example, consider the add $0,$1,$2 instruction.      */
/*  In binary, this instruction is:                          */
/*   000000 00001 00010 00000 00000 100000                   */
/*  The unsigned representation in decimal is therefore:     */
/*   2228256                                                 */
/*************************************************************/
unsigned int instrToInt(char* inst, char* args){

    int opcode, rs, rt, rd, shamt, funct, immed;
    unsigned int dec_inst;
    
	if((strcmp(inst, "add") == 0) || (strcmp(inst, "sub") == 0) || (strcmp(inst, "and") == 0) || (strcmp(inst, "mov") == 0)){
        	opcode = 0;
			rd = atoi(strtok(args, ",$"));
        	rs = atoi(strtok(NULL, ",$"));
        	rt = atoi(strtok(NULL, ",$"));
        	if(strcmp(inst, "add") == 0)
			{funct = ADD;}
        	else if (strcmp(inst, "sub") == 0)
            {funct = SUB;} 
			else if (strcmp(inst, "and") == 0)
			{funct = AND;}
			else 
			{funct = 38;
			rt = rs;}
        	shamt = 0; 
        	

        	dec_inst = (opcode << 26) + (rs << 21) + (rt << 16) + (rd << 11) + (shamt << 6) + funct;
    		} 
	else if((strcmp(inst, "lw") == 0) || (strcmp(inst, "sw") == 0)){
        	if(strcmp(inst, "lw") == 0)
            		opcode = LW;
        	else
            		opcode = SW;

        	rt = atoi(strtok(args, ",$"));
        	immed = atoi(strtok(NULL, ",("));
        	rs = atoi(strtok(NULL, "($)"));
        	dec_inst = (opcode << 26) + (rs << 21) + (rt << 16) + immed;

		} 
	else if(strcmp(inst, "beq") == 0){
        	opcode = 4;
        	rs = atoi(strtok(args, ",$"));
        	rt = atoi(strtok(NULL, ",$"));
		immed = atoi(strtok(NULL, ","));
		dec_inst = (opcode << 26) + (rs << 21) + (rt << 16) + immed;   
    		} 
	else if(strcmp(inst, "halt") == 0){
        	opcode = 63; 
        	dec_inst = (opcode << 26);
    		} 
	else if(strcmp(inst, "noop") == 0){
        	dec_inst = 0;
    		}

	else if(strcmp(inst, "not") == 0){
		opcode = 38;
        funct = 0;
        shamt = 0; 
        rd = atoi(strtok(args, ",$"));
        rs = 0;
        rt = 0;

        dec_inst = (opcode << 26) + (rs << 21) + (rt << 16) + (rd << 11) + (shamt << 6) + funct;
	}

return dec_inst;
}

///////////////////////////////////////////////////////////////
//extract fields from encoded bit representation of instruction
int get_rs(unsigned int instruction){
    return( (instruction>>21) & 0x1F);
}

int get_rt(unsigned int instruction){
    return( (instruction>>16) & 0x1F);
}

int get_rd(unsigned int instruction){
    return( (instruction>>11) & 0x1F);
}

int get_funct(unsigned int instruction){
    return(instruction & 0x3F);
}

int get_immed(unsigned int instruction){
    return(instruction & 0xFFFF);
}

int get_opcode(unsigned int instruction){
    return(instruction>>26);
}
///////////////////////////////////////////////////////////////

/*************************************************/
/*  The printInstruction decodes an unsigned     */
/*  integer representation of an instruction     */
/*  into its string representation and prints    */
/*  the result to stdout.                        */
/*************************************************/
void printInstruction(unsigned int instr)
{
char opcodeString[10];

if (instr == 0){
	printf("NOOP\n");
    	} 
else if (get_opcode(instr) == R) {

       	if(get_funct(instr)!=0){

      		if(get_funct(instr) == ADD)
             		strcpy(opcodeString, "add");
       		else
			strcpy(opcodeString, "sub");

			printf("%s $%d,$%d,$%d\n", opcodeString, get_rd(instr), get_rs(instr), get_rt(instr));
        	}
        	else{
			printf("NOOP\n");
        		}
    	}
else if (get_opcode(instr) == LW) {
        printf("%s $%d,%d($%d)\n", "lw", get_rt(instr), get_immed(instr), get_rs(instr));
    }
else if (get_opcode(instr) == SW) {
        printf("%s $%d,%d($%d)\n", "sw", get_rt(instr), get_immed(instr), get_rs(instr));
    }
else if (get_opcode(instr) == BEQ) {
        printf("%s $%d,$%d,%d\n", "beq", get_rs(instr), get_rt(instr), get_immed(instr));
    }
else if (get_opcode(instr) == HALT) {
        printf("%s\n", "halt");
    }
}

