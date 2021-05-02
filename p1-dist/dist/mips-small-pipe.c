#include "mips-small-pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Macros to clean up code*/
#define OPEQ(X,Y) (opcode(X) == Y)
#define REGREG(X) (OPEQ(X,REG_REG_OP))
#define LW(X) (OPEQ(X,LW_OP))
#define SW(X) (OPEQ(X,SW_OP))
#define ADDI(X) (OPEQ(X,ADDI_OP))
#define BEQZ(X) (OPEQ(X,BEQZ_OP))
#define HALT(X) (OPEQ(X,HALT_OP))


/************************************************************/
int main(int argc, char *argv[]) {
  short i;
  char line[MAXLINELENGTH];
  state_t state;
  FILE *filePtr;

  if (argc != 2) {
    printf("error: usage: %s <machine-code file>\n", argv[0]);
    return 1;
  }

  memset(&state, 0, sizeof(state_t));

  state.pc = state.cycles = 0;
  state.IFID.instr = state.IDEX.instr = state.EXMEM.instr = state.MEMWB.instr =
      state.WBEND.instr = NOPINSTRUCTION; /* nop */

  /* read machine-code file into instruction/data memory (starting at address 0)
   */

  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL) {
    printf("error: can't open file %s\n", argv[1]);
    perror("fopen");
    exit(1);
  }

  for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
       state.numMemory++) {
    if (sscanf(line, "%x", &state.dataMem[state.numMemory]) != 1) {
      printf("error in reading address %d\n", state.numMemory);
      exit(1);
    }
    state.instrMem[state.numMemory] = state.dataMem[state.numMemory];
    printf("memory[%d]=%x\n", state.numMemory, state.dataMem[state.numMemory]);
  }

  printf("%d memory words\n", state.numMemory);

  printf("\tinstruction memory:\n");
  for (i = 0; i < state.numMemory; i++) {
    printf("\t\tinstrMem[ %d ] = ", i);
    printInstruction(state.instrMem[i]);
  }

  run(&state);

  return 0;
}
/************************************************************/

/************************************************************/
void run(Pstate state) {
  state_t new;
  
  memset(&new, 0, sizeof(state_t));

  while (1) {
    
    printState(state);
    /*Halt at MEMWB register*/
    if (opcode(new.MEMWB.instr) == HALT_OP) {
      printf("machine halted\ntotal of %d cycles executed\n", new.cycles);
      break;
    }

    /* copy everything so all we have to do is make changes.
       (this is primarily for the memory and reg arrays) */
    memcpy(&new, state, sizeof(state_t));

    new.cycles++;
    /* --------------------- IF stage --------------------- */
    new.IFID.instr = new.instrMem[new.pc / 4];
    new.pc += 4;
    new.IFID.pcPlus1 = new.pc;
    if (opcode(new.IFID.instr) == BEQZ_OP 
    && offset(new.IFID.instr) <= 0) {
      new.pc += offset(new.IFID.instr);
    }
    /* --------------------- ID stage --------------------- */
    if (opcode(state -> IDEX.instr) == LW_OP
    && (field_r2(state -> IDEX.instr) == field_r1(state -> IFID.instr)
    || (field_r2(state -> IDEX.instr) == field_r2(state -> IFID.instr) 
    && !LW(state->IFID.instr) && !ADDI(state->IFID.instr)))) {
      new.pc = state -> IFID.pcPlus1;
      new.IFID = state -> IFID;
      new.IDEX.instr = NOPINSTRUCTION;
      new.IDEX.pcPlus1 = 0;
    } else {
      new.IDEX.instr = state -> IFID.instr;
      new.IDEX.pcPlus1 = state -> IFID.pcPlus1;
    }
    
    new.IDEX.offset = offset(new.IDEX.instr);
    new.IDEX.readRegA = new.reg[field_r1(new.IDEX.instr)];
    new.IDEX.readRegB = new.reg[field_r2(new.IDEX.instr)];
    
    /*Check if previous instr is a load that uses a source reg here*/
    /* --------------------- EX stage --------------------- */
    new.EXMEM.instr = state -> IDEX.instr;
    if (new.EXMEM.instr != NOPINSTRUCTION) {

      /*HAZARD CHECK*/
      state -> IDEX = *regFwd(new.EXMEM.instr, 
        state -> WBEND.instr, state -> WBEND.writeData, state);
      state -> IDEX = *regFwd(new.EXMEM.instr,
        state -> MEMWB.instr, state -> MEMWB.writeData, state);
      state -> IDEX = *regFwd(new.EXMEM.instr, 
        state -> EXMEM.instr, state -> EXMEM.aluResult, state);
      
      /*ALU*/
      switch (opcode(new.EXMEM.instr)) {
        case REG_REG_OP:
          /*ALU Operations*/
          switch (func(new.EXMEM.instr)) {
            case ADD_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA + state -> IDEX.readRegB;
              break;
            case SLL_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA << state -> IDEX.readRegB;
              break;
            case SRL_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA >> state -> IDEX.readRegB;
              break;
            case SUB_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA - state -> IDEX.readRegB;
              break;
            case AND_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA & state -> IDEX.readRegB;
              break;
            case OR_FUNC:
              new.EXMEM.aluResult = 
              state -> IDEX.readRegA | state -> IDEX.readRegB;
              break;
          }
          break;
        case ADDI_OP:
          new.EXMEM.aluResult = 
          state -> IDEX.readRegA + state -> IDEX.offset;
          break;
        case BEQZ_OP:
          new.EXMEM.aluResult = state -> IDEX.pcPlus1 + state -> IDEX.offset;
          if ((state -> IDEX.offset < 0 
          && state -> IDEX.readRegA != 0)
          || (state -> IDEX.offset > 0
          && state -> IDEX.readRegA == 0)) {
            new.IFID.instr = NOPINSTRUCTION;
            new.IFID.pcPlus1 = 0;
            new.IDEX.instr = NOPINSTRUCTION;
            new.IDEX.pcPlus1 = 0;
            new.IDEX.readRegA = 0;
            new.IDEX.readRegB = 0;
            new.IDEX.offset = 32;
            if (state -> IDEX.offset < 0) {
              new.pc = state -> IDEX.pcPlus1;
            } else {
              new.pc = state -> IDEX.offset + state -> IDEX.pcPlus1;
            }
          }
          break;
        case LW_OP:
        case SW_OP:
          new.EXMEM.aluResult = state -> IDEX.readRegA + state -> IDEX.offset;
          break;
        default:
          new.EXMEM.aluResult = 0;
          break;
      }
      new.EXMEM.readRegB = state -> IDEX.readRegB;
    } else {
      new.EXMEM.aluResult = 0;
    }
    /* --------------------- MEM stage --------------------- */
    new.MEMWB.instr = state -> EXMEM.instr;
    switch (opcode(new.MEMWB.instr)) {
      case LW_OP:
        new.MEMWB.writeData = new.dataMem[state -> EXMEM.aluResult/4];
        break;
      case SW_OP:
        new.MEMWB.writeData = state -> EXMEM.readRegB;
        new.dataMem[state -> EXMEM.aluResult/4] = state -> EXMEM.readRegB;
        break;
      default:
        new.MEMWB.writeData = state -> EXMEM.aluResult;
    }
    /* --------------------- WB stage --------------------- */
    new.WBEND.instr = state -> MEMWB.instr;
    new.WBEND.writeData = state -> MEMWB.writeData;

    switch (opcode(new.WBEND.instr)) {
      case REG_REG_OP:
        new.reg[field_r3(new.WBEND.instr)] = new.WBEND.writeData;
        break;
      case LW_OP:
      case ADDI_OP:
        new.reg[field_r2(new.WBEND.instr)] = new.WBEND.writeData;
    }

    /* --------------------- end stage --------------------- */
    /* transfer new state into current state */
    memcpy(state, &new, sizeof(state_t));
  }
}
/************************************************************/
/*Helper Functions*/
/*Checks if the id stage has a RAW Hazard and returns the correct IFID register*/
IDEX_t* regFwd(int idInst, int fwdInst, int fwdRes, Pstate state) {
  /*HAZARD CHECK*/
  if (REGREG(fwdInst)) {
    if (field_r1(idInst) == field_r3(fwdInst)) {
      state -> IDEX.readRegA = fwdRes;
    } else if (field_r2(idInst) == field_r3(fwdInst)
    && !LW(idInst) && !ADDI(idInst)) {
      state -> IDEX.readRegB = fwdRes;
    }
  } else if (ADDI(fwdInst) || LW(fwdInst)) {
    if (field_r1(idInst) == field_r2(fwdInst)) {
      state -> IDEX.readRegA = fwdRes;
    } else if (field_r2(idInst) == field_r2(fwdInst) 
    && !ADDI(idInst) && !LW(idInst)) {
      state -> IDEX.readRegB = fwdRes;
    }
  } 
  return &state -> IDEX;
}
/************************************************************/
int opcode(int instruction) { return (instruction >> OP_SHIFT) & OP_MASK; }
/************************************************************/

/************************************************************/
int func(int instruction) { return (instruction & FUNC_MASK); }
/************************************************************/

/************************************************************/
int field_r1(int instruction) { return (instruction >> R1_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_r2(int instruction) { return (instruction >> R2_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_r3(int instruction) { return (instruction >> R3_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_imm(int instruction) { return (instruction & IMMEDIATE_MASK); }
/************************************************************/

/************************************************************/
int offset(int instruction) {
  /* only used for lw, sw, beqz */
  return convertNum(field_imm(instruction));
}
/************************************************************/

/************************************************************/
int convertNum(int num) {
  /* convert a 16 bit number into a 32-bit Sun number */
  if (num & 0x8000) {
    num -= 65536;
  }
  return (num);
}
/************************************************************/

/************************************************************/
void printState(Pstate state) {
  short i;
  printf("@@@\nstate before cycle %d starts\n", state->cycles);
  printf("\tpc %d\n", state->pc);
  
  printf("\tdata memory:\n");
  for (i = 0; i < state->numMemory; i++) {
    printf("\t\tdataMem[ %d ] %d\n", i, state->dataMem[i]);
  }
  printf("\tregisters:\n");
  for (i = 0; i < NUMREGS; i++) {
    printf("\t\treg[ %d ] %d\n", i, state->reg[i]);
  }
  printf("\tIFID:\n");
  printf("\t\tinstruction ");
  printInstruction(state->IFID.instr);
  printf("\t\tpcPlus1 %d\n", state->IFID.pcPlus1);
  printf("\tIDEX:\n");
  printf("\t\tinstruction ");
  printInstruction(state->IDEX.instr);
  printf("\t\tpcPlus1 %d\n", state->IDEX.pcPlus1);
  printf("\t\treadRegA %d\n", state->IDEX.readRegA);
  printf("\t\treadRegB %d\n", state->IDEX.readRegB);
  printf("\t\toffset %d\n", state->IDEX.offset);
  printf("\tEXMEM:\n");
  printf("\t\tinstruction ");
  printInstruction(state->EXMEM.instr);
  printf("\t\taluResult %d\n", state->EXMEM.aluResult);
  printf("\t\treadRegB %d\n", state->EXMEM.readRegB);
  printf("\tMEMWB:\n");
  printf("\t\tinstruction ");
  printInstruction(state->MEMWB.instr);
  printf("\t\twriteData %d\n", state->MEMWB.writeData);
  printf("\tWBEND:\n");
  printf("\t\tinstruction ");
  printInstruction(state->WBEND.instr);
  printf("\t\twriteData %d\n", state->WBEND.writeData);
}
/************************************************************/

/************************************************************/
void printInstruction(int instr) {

  if (opcode(instr) == REG_REG_OP) {

    if (func(instr) == ADD_FUNC) {
      print_rtype(instr, "add");
    } else if (func(instr) == SLL_FUNC) {
      print_rtype(instr, "sll");
    } else if (func(instr) == SRL_FUNC) {
      print_rtype(instr, "srl");
    } else if (func(instr) == SUB_FUNC) {
      print_rtype(instr, "sub");
    } else if (func(instr) == AND_FUNC) {
      print_rtype(instr, "and");
    } else if (func(instr) == OR_FUNC) {
      print_rtype(instr, "or");
    } else {
      printf("data: %d\n", instr);
    }

  } else if (opcode(instr) == ADDI_OP) {
    print_itype(instr, "addi");
  } else if (opcode(instr) == LW_OP) {
    print_itype(instr, "lw");
  } else if (opcode(instr) == SW_OP) {
    print_itype(instr, "sw");
  } else if (opcode(instr) == BEQZ_OP) {
    print_itype(instr, "beqz");
  } else if (opcode(instr) == HALT_OP) {
    printf("halt\n");
  } else {
    printf("data: %d\n", instr);
  }
}
/************************************************************/

/************************************************************/
void print_rtype(int instr, const char *name) {
  printf("%s %d %d %d\n", name, field_r3(instr), field_r1(instr),
         field_r2(instr));
}
/************************************************************/

/************************************************************/
void print_itype(int instr, const char *name) {
  printf("%s %d %d %d\n", name, field_r2(instr), field_r1(instr),
         offset(instr));
}
/************************************************************/
