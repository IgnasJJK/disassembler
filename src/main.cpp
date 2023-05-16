#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.cpp"
#include "disassembly.cpp"

#include "simulation.cpp"

static char* const registers8bit[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
static char* const registers16bit[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
static char* const registersSegment[] = {"es", "cs", "ss", "ds"};
static char* const effectiveAddressTable[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

inline u8 Load8BitValue(FILE* file)
{
    return (u8)fgetc(file);
}

inline u16 Load16BitValue(FILE* file)
{
    u16 value = 0;
    *((u8 *)(&value) + 0) = (u8)fgetc(file);
    *((u8 *)(&value) + 1) = (u8)fgetc(file);
    return value;
}

/// @brief Loads memory 
/// @param file handle to assembled file
/// @param[out] operand output location for operand data
void LoadMemoryOperand(FILE* file, Operand* operand, OperandByte operandByte)
{
    // TODO: This function changes the operand type for registers.
    // Maybe it should do the same with memory operands.

    operand->type = OP_MEMORY;
    operand->modField = operandByte.mod;
    operand->regmemIndex = operandByte.rm;

    switch (operand->modField)
    {
        case REGISTER_MODE:
            operand->type = OP_REGISTER;
        break;
        case MEMORY_8BIT_MODE:
            operand->valueLow = (i8)Load8BitValue(file);
        break;
        case MEMORY_0BIT_MODE:
            if (operand->regmemIndex != MEM_DIRECT) break;
        // fallthrough
        case MEMORY_16BIT_MODE:
                operand->value = (i16)Load16BitValue(file);
        break;
    }
}

/// @brief Loads immediate operand value
/// @param file handle to assembled file
/// @param[out] operand operand to load value into
/// @param wideOperation if true, 2 bytes are loaded. Otherwise - 1 byte.
/// @param signExtend if true, loads 1 byte and sign extends it to 2 bytes
void LoadImmediateOperand(FILE* file, Operand* operand, bool wideOperation, bool signExtend)
{
    operand->type = OP_IMMEDIATE;

    if (signExtend)
    {
        operand->valueLow = (i8)Load8BitValue(file);
        if ((operand->valueLow >> 7) & 0b1)
        {
            operand->valueHigh = 127i8;
        }
    }
    else if (wideOperation)
    {
        operand->value = (i16)Load16BitValue(file);
    }
    else
    {
        operand->valueLow = (i8)Load8BitValue(file);
    }
}

#define global_variable static

void PrintAddressOperand(char* effectiveAddress, i8 displacement)
{
    if (displacement == 0)
    {
        printf("[%s]", effectiveAddress);
    }
    else
    {
        printf("[%s %s %d]", effectiveAddress, 
            (displacement >= 0 ? "+" : "-"), 
            (displacement < 0 ? -displacement : displacement));
    }
}

void PrintAddressOperand(char* effectiveAddress, i16 displacement)
{
    if (displacement == 0)
    {
        printf("[%s]", effectiveAddress);
    }
    else
    {
        printf("[%s %s %d]", effectiveAddress, 
            (displacement >= 0 ? "+" : "-"), 
            (displacement < 0 ? -displacement : displacement));
    }
}

void PrintOperand(Operand operand, bool wideOperation)
{
    switch (operand.type)
    {
        case OP_REGISTER:
        {
            // NOTE: Size of registers is implicitly known, so size specification is not needed
            char* const* registerNames = (wideOperation? registers16bit : registers8bit);
            printf("%s", registerNames[operand.regmemIndex]);
        }
        break;
        case OP_SEGMENT_REGISTER:
        {
            printf("%s", registersSegment[operand.regmemIndex]);
        }
        break;
        case OP_IMMEDIATE:
        {
            if (operand.outputWidth)
            {
                printf(wideOperation? "word " : "byte ");
            }
            printf("%d", (wideOperation? (i16)operand.value : (i8)operand.valueLow));
        }
        break;
        case OP_MEMORY:
        {
            if (operand.outputWidth)
            {
                printf(wideOperation? "word " : "byte ");
            }
            if (operand.modField == MEMORY_0BIT_MODE)
            {
                if (operand.regmemIndex == MEM_DIRECT)
                {
                    printf("[%d]", (i16)operand.value);
                }
                else
                {
                    printf("[%s]", effectiveAddressTable[operand.regmemIndex]);
                }
            }
            else if (operand.modField == MEMORY_8BIT_MODE)
            {
                PrintAddressOperand(effectiveAddressTable[operand.regmemIndex], (i8)operand.valueLow);
            }
            else if (operand.modField == MEMORY_16BIT_MODE)
            {
                PrintAddressOperand(effectiveAddressTable[operand.regmemIndex], (i16)operand.value);
            }
            else
            {
                printf("; error: memory operand in register mode\n");
            }
        }
        break;
    }
}

void PrintInstruction(Instruction* inst)
{
    Assert(inst->operandCount >= 0 && inst->operandCount <= 2);

    printf(operationNames[inst->type]);
    printf(" ");

    switch (inst->type)
    {
        case DIS_JO:
        case DIS_JNO:
        case DIS_JB:
        case DIS_JNB:
        case DIS_JE:
        case DIS_JNE:
        case DIS_JBE:
        case DIS_JNBE:
        case DIS_JS:
        case DIS_JNS:
        case DIS_JP:
        case DIS_JNP:
        case DIS_JL:
        case DIS_JNL:
        case DIS_JLE:
        case DIS_JNLE:
        case DIS_LOOP:
        case DIS_LOOPZ:
        case DIS_LOOPNZ:
        case DIS_JCXZ:
        {
            i8 displacement = inst->opDest.valueLow;
            if (displacement >= 0)
            {
                printf(" $+%d", displacement);
            }
            else
            {
                printf(" $%d", displacement);
            }
        }
        break;
        
        case DIS_SHL:
        case DIS_SHR:
        case DIS_SAR:
        case DIS_ROL:
        case DIS_ROR:
        case DIS_RCL:
        case DIS_RCR:
        {
            PrintOperand(inst->opDest, inst->isWide);
            printf(", ");
            PrintOperand(inst->opSrc, false);
        }
        break;

        case DIS_IN:
        {
            PrintOperand(inst->opDest, inst->isWide);
            printf(", ");
            PrintOperand(inst->opSrc, true);
        } 
        break;
        case DIS_OUT:
        {
            PrintOperand(inst->opDest, true);
            printf(", ");
            PrintOperand(inst->opSrc, inst->isWide);
        }
        break;

        default:
        {
            if (inst->operandCount == 1)
            {
                PrintOperand(inst->opDest, inst->isWide);
            }
            else if (inst->operandCount == 2)
            {
                PrintOperand(inst->opDest, inst->isWide);
                printf(", ");
                PrintOperand(inst->opSrc, inst->isWide);
            }

        }
        break;
    }
}


void PrintBinary(u16 value)
{
    u16 index = (1 << 15);
    while(index)
    {
        if (index == (1 << 7))
            printf(" ");

        printf((value & index) ? "1" : "0");
        index >>= 1;
    }
}

bool DecodeSingleByteInstruction(u8 opcode, Instruction* instruction)
{
    switch (opcode)
    {
        case INST_XLAT:  instruction->type = DIS_XLAT; break;
        case INST_DAA:   instruction->type = DIS_DAA; break;
        case INST_AAA:   instruction->type = DIS_AAA; break;
        case INST_AAS:   instruction->type = DIS_AAS; break;
        case INST_DAS:   instruction->type = DIS_DAS; break;
        case INST_CBW:   instruction->type = DIS_CBW; break;
        case INST_CWD:   instruction->type = DIS_CWD; break;
        case INST_INTO:  instruction->type = DIS_INTO; break;
        case INST_IRET:  instruction->type = DIS_IRET; break;
        case INST_CLC:   instruction->type = DIS_CLC; break;
        case INST_CMC:   instruction->type = DIS_CMC; break;
        case INST_STC:   instruction->type = DIS_STC; break;
        case INST_CLD:   instruction->type = DIS_CLD; break;
        case INST_STD:   instruction->type = DIS_STD; break;
        case INST_CLI:   instruction->type = DIS_CLI; break;
        case INST_STI:   instruction->type = DIS_STI; break;
        case INST_HLT:   instruction->type = DIS_HLT; break;
        case INST_WAIT:  instruction->type = DIS_WAIT; break;
        case INST_PUSHF: instruction->type = DIS_PUSHF; break;
        case INST_POPF:  instruction->type = DIS_POPF; break;
        case INST_SAHF:  instruction->type = DIS_SAHF; break;
        case INST_LAHF:  instruction->type = DIS_LAHF; break;
        case INST_LOCK:  instruction->type = DIS_LOCK; break;

        case INST_MOVSB: instruction->type = DIS_MOVSB; break;
        case INST_MOVSW: instruction->type = DIS_MOVSW; break;
        case INST_CMPSB: instruction->type = DIS_CMPSB; break;
        case INST_CMPSW: instruction->type = DIS_CMPSW; break;
        case INST_SCASB: instruction->type = DIS_SCASB; break;
        case INST_SCASW: instruction->type = DIS_SCASW; break;
        case INST_LODSB: instruction->type = DIS_LODSB; break;
        case INST_LODSW: instruction->type = DIS_LODSW; break;
        case INST_STOSB: instruction->type = DIS_STOSB; break;
        case INST_STOSW: instruction->type = DIS_STOSW; break;

        case INST_INT3:
            instruction->type = DIS_INT;
            instruction->operandCount = 1;
            instruction->opDest = InitImmediateOperand(3);
            break;

        case INST_RET_INTERSEGMENT: 
        case INST_RET_WITHIN_SEGMENT: 
            instruction->type = DIS_RET; 
            break;
        
        default: return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    InstructionType instructionSubtypes[] = {
        DIS_ADD, DIS_OR, DIS_ADC, DIS_SBB, DIS_AND, DIS_SUB, DIS_XOR, DIS_CMP
    };

    bool execute = false;
    CPU cpu {0};

    if (argc > 2)
    {
        for (int argIndex = 1; argIndex < argc - 1; ++argIndex)
        {
            char* arg = argv[argIndex];

            if (strcmp("-e", arg) == 0 || strcmp("-E", arg) == 0)
            {
                execute = true;
            }
        }
    }

    if (argc >= 2)
    {
        char* fileName = argv[argc - 1];
        FILE* file;
        fopen_s(&file, fileName, "rb"); // TODO: Handle returned error?

        if (file)
        {
            printf("; Disassembly: %s\n", fileName);
            printf("bits 16\n");

            int input;
            // NOTE: fgetc is important here otherwise the loop might not end.
            while((input = fgetc(file)) != EOF)
            {
                u8 opcode = (u8)input;

                Instruction instruction {0};

                if (DecodeSingleByteInstruction(opcode, &instruction))
                {
                    // Success
                }
                else if (opcode == INST_AAM) // AAM
                {
                    Load8BitValue(file);
                    // STUDY: It's supposed to be 0b00001010, but it's different. Trash data?
                    //Assert(nextByte == 0b00001010);
                    instruction.type = DIS_AAM;
                }
                else if (opcode == INST_AAD) // AAD
                {
                    Load8BitValue(file);
                    // STUDY: It's supposed to be 0b00001010, but it's different. Trash data?
                    //Assert(nextByte == 0b00001010);
                    instruction.type = DIS_AAD;
                }
                else if (opcode == INST_LEA || opcode == INST_LDS || opcode == INST_LES)
                {
                    OperandByte instOperand = Inst_ParseOperand(Load8BitValue(file));

                    if (opcode == INST_LEA)
                        instruction.type = DIS_LEA;
                    else if (opcode == INST_LDS)
                        instruction.type = DIS_LDS;
                    else
                        instruction.type = DIS_LES;

                    instruction.isWide = true;
                    instruction.operandCount = 2;
                    instruction.opDest = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.opSrc, instOperand);
                }
                else if (opcode == INST_INT)
                {
                    instruction.type = DIS_INT;
                    instruction.operandCount = 1;
                    instruction.opDest = InitImmediateOperand(Load8BitValue(file));
                }
                else if (opcode == INST_MOV_REGMEM_SR || opcode == INST_MOV_SR_REGMEM)
                {
                    OperandByte operand = Inst_ParseOperand(Load8BitValue(file));
                    instruction.type = DIS_MOV;
                    instruction.operandCount = 2;
                    instruction.isWide = true;

                    bool toSegmentRegister = ((opcode >> 1) & 0b1);

                    Operand* segment;
                    Operand* regmem;
                    if (toSegmentRegister)
                    {
                        segment = &instruction.opDest;
                        regmem = &instruction.opSrc;
                    }
                    else
                    {
                        segment = &instruction.opSrc;
                        regmem = &instruction.opDest;
                    }

                    segment->type = OP_SEGMENT_REGISTER;
                    segment->regmemIndex = operand.reg;
                    LoadMemoryOperand(file, regmem, operand);
                }
                else if ((opcode & 0b11111110) == 0b11110010) instruction.type = DIS_REP;
                else if ((opcode & 0b11000100) == 0b00000000)
                {
                    instruction.isWide = (opcode & 0b1);
                    instruction.type = instructionSubtypes[((opcode >> 3) & 0b111)];

                    bool directionBit = (opcode >> 1) & 0b1;
                    OperandByte instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;
                    Operand *op1;
                    Operand *op2;

                    if (directionBit)
                    {
                        op1 = &instruction.opDest;
                        op2 = &instruction.opSrc;
                    }
                    else
                    {
                        op2 = &instruction.opDest;
                        op1 = &instruction.opSrc;
                    }
                    *op1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, op2, instOperand);
                }
                else if ((opcode & 0b11110000) == 0b01010000) // Push/pop register
                {
                    bool isPop = ((opcode >> 3) & 0b1);
                    RMField reg = (RMField)(opcode & 0b111);

                    instruction.type = (isPop? DIS_POP : DIS_PUSH);
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.opDest = InitRegisterOperand(reg);
                }
                else if ((opcode & 0b11100110) == 0b00000110) // Push/pop segment register
                {
                    bool isPop = (opcode & 0b1);
                    u8 segreg = ((opcode >> 3) & 0b11);

                    instruction.type = (isPop? DIS_POP : DIS_PUSH);
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.opDest = InitSegmentRegisterOperand((RMField)segreg);
                }
                else if ((opcode & 0b11110100) == 0b11100100) // IN/OUT fixed port
                {
                    bool variableBit = ((opcode >> 3) & 0b1);
                    bool typeBit = ((opcode >> 1) & 0b1);

                    instruction.type = (typeBit? DIS_OUT : DIS_IN);
                    instruction.isWide = (opcode & 0b1);

                    // FIXME: Operands should be unsigned
                    instruction.operandCount = 2;
                    Operand* op1;
                    Operand* op2;
                    
                    if (typeBit)
                    {
                        op2 = &instruction.opDest;
                        op1 = &instruction.opSrc;
                    }
                    else
                    {
                        op1 = &instruction.opDest;
                        op2 = &instruction.opSrc;
                    }

                    *op1 = InitRegisterOperand(REG_AX);
                    if (variableBit)
                    {
                        *op2 = InitRegisterOperand(REG_DX);
                    }
                    else
                    {
                        LoadImmediateOperand(file, op2, false, false);
                    }
                }
                else if ((opcode & 0b11111100) == (0b10000100))
                {
                    instruction.isWide = (opcode & 0b1);
                    instruction.type = ((opcode >> 1) & 0b1) ? DIS_XCHG : DIS_TEST;

                    OperandByte instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;
                    instruction.opDest = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.opSrc, instOperand);
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_XCHG_ACC_WITH_REG)
                {
                    instruction.type = DIS_XCHG;
                    instruction.operandCount = 2;
                    instruction.isWide = true;
                    instruction.opDest = InitRegisterOperand(REG_AX);
                    instruction.opSrc = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_INC_REG)
                {
                    instruction.type = DIS_INC;
                    instruction.isWide = true;
                    instruction.operandCount = 1;
                    instruction.opDest = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_DEC_REG)
                {
                    instruction.type = DIS_DEC;
                    instruction.isWide = true;
                    instruction.operandCount = 1;
                    instruction.opDest = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & 0b11000100) == 0b00000100) // Immediate to accumulator
                {
                    instruction.isWide = (opcode & 0b1);
                    u8 instructionType = ((opcode >> 3) & 0b111);
                    instruction.type = instructionSubtypes[instructionType];

                    instruction.operandCount = 2;
                    instruction.opDest = InitRegisterOperand(REG_AX); // Same ID as REG_AL
                    LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, false);
                }
                else if ((opcode & 0b11111100) == 0b10000000) // Immediate to register/memory
                {
                    bool signBit = ((opcode >> 1) & 0b1);
                    instruction.isWide = (opcode & 0b1);

                    OperandByte instructionOperand = Inst_ParseOperand(Load8BitValue(file));

                    // NOTE: For immediate instructions, the REG part of the operand byte determines the operation type.
                    instruction.type = instructionSubtypes[instructionOperand.reg];

                    instruction.operandCount = 2;
                    instruction.opDest.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.opDest, instructionOperand);
                    LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, signBit);
                }
                else if ((opcode & 0b11111100) == 0b11010000) // Logic operations
                {
                    InstructionType logicSubtypes[] = {DIS_ROL, DIS_ROR, DIS_RCL, DIS_RCR, DIS_SHL, DIS_SHR, DIS_NOOP, DIS_SAR};
                    OperandByte operand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.type = logicSubtypes[operand.reg];
                    instruction.isWide = (opcode & 0b1);

                    instruction.operandCount = 2;
                    instruction.opDest.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.opDest, operand);

                    bool shiftByCLValue = ((opcode >> 1) & 0b1);
                    if (shiftByCLValue)
                    {
                        instruction.opSrc = InitRegisterOperand(REG_CX);
                    }
                    else
                    {
                        instruction.opSrc = InitImmediateOperand(1);
                    }
                }
                else if ((opcode & 0b11111110) == 0b11000110) // MOVE immediate to register/memory
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = (opcode & 0b1);

                    OperandByte instOperand = Inst_ParseOperand((u8)Load8BitValue(file));

                    Assert(instOperand.reg == 0b000);// NOTE: Other reg values are not used. 

                    instruction.operandCount = 2;
                    instruction.opSrc.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.opDest, instOperand);
                    LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, false);
                }
                else if ((opcode & 0b11111100) == 0b10001000)
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = (opcode & 0b1);
                    bool switchOperands = ((opcode >> 1) & 0b1);

                    OperandByte instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;

                    Operand* op1;
                    Operand* op2;
                    if (switchOperands)
                    {
                        op1 = &instruction.opDest;
                        op2 = &instruction.opSrc;
                    }
                    else
                    {
                        op1 = &instruction.opSrc;
                        op2 = &instruction.opDest;
                    }
                    *op1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, op2, instOperand);
                }
                else if ((opcode & 0b11111100) == 0b10100000) // MOV accumulator-memory
                {
                    // NOTE: The direction bit here has opposite meaning to the standard one, and it's written as 2 separate commands in the manual.
                    instruction.type = DIS_MOV;
                    bool directionBit = ((opcode >> 1) & 0b1);
                    instruction.isWide = (opcode & 0b1);

                    instruction.operandCount = 2;
                    Operand* op1;
                    Operand* op2;

                    if (directionBit)
                    {
                        op2 = &instruction.opDest;
                        op1 = &instruction.opSrc;
                    }
                    else
                    {
                        op1 = &instruction.opDest;
                        op2 = &instruction.opSrc;
                    }

                    *op1 = InitRegisterOperand(REG_AX);
                    op2->type = OP_MEMORY;
                    op2->regmemIndex = MEM_DIRECT;
                    op2->value = (i16)Load16BitValue(file);
                }
                else if (opcode == 0b11000010) // RET - within seg adding immediate to SP
                {
                    instruction.type = DIS_RET;
                    instruction.operandCount = 1;
                    instruction.opDest = InitImmediateOperand((i16)Load16BitValue(file));
                }
                else if ((opcode & 0b11110000) == 0b10110000) // MOV imm -> reg
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = ((opcode >> 3) & 0b1);
                    instruction.operandCount = 2;
                    instruction.opDest = InitRegisterOperand((RMField)(opcode & 0b111));
                    LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, false);
                }
                else if ((opcode & 0b11110000) == 0b01110000)
                {
                    InstructionType jumpSuptypes[] = {
                        DIS_JO, DIS_JNO, DIS_JB, DIS_JNB, DIS_JE, DIS_JNE, DIS_JBE, DIS_JNBE,
                        DIS_JS, DIS_JNS, DIS_JP, DIS_JNP, DIS_JL, DIS_JNL, DIS_JLE, DIS_JNLE
                    };
                    // TODO: Jump labels

                    u8 jumpType = (opcode & 0b1111);

                    instruction.type = jumpSuptypes[jumpType];
                    instruction.operandCount = 1;
                    instruction.opDest = InitImmediateOperand(((i8)Load8BitValue(file) + 2));
                }
                else if ((opcode & 0b11111100) == 0b11100000)
                {
                    InstructionType loopTypes[] = { DIS_LOOPNZ, DIS_LOOPZ, DIS_LOOP, DIS_JCXZ};

                    instruction.type = loopTypes[(opcode & 0b11)];
                    instruction.operandCount = 1;
                    instruction.opDest = InitImmediateOperand((i8)Load8BitValue(file) + 2);
                }
                else if ((opcode & 0b11111111) == 0b10001111) // pop
                {
                    OperandByte instOperand = Inst_ParseOperand(Load8BitValue(file));
                    Assert(instOperand.reg == 0b000); // NOTE: Other values are not used.
                    
                    instruction.type = DIS_POP;
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.opDest.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.opDest, instOperand);
                }
                else if ((opcode & 0b11111110) == 0b11111110)
                {
                    InstructionType instructionSubtypesInc[] = {DIS_INC, DIS_DEC, DIS_CALL, DIS_CALL, DIS_JMP, DIS_JMP, DIS_PUSH};

                    OperandByte instructionOperand = Inst_ParseOperand(Load8BitValue(file));

                    Assert(instructionOperand.reg != 0b111);

                    instruction.isWide = (opcode & 0b1);
                    instruction.type = instructionSubtypesInc[instructionOperand.reg];

                    instruction.operandCount = 1;
                    LoadMemoryOperand(file, &instruction.opDest, instructionOperand);

                    if (instructionOperand.reg == 0b110 || 
                        instructionOperand.reg == 0b000 || 
                        instructionOperand.reg == 0b001) // push/inc/dec
                    {
                        instruction.opDest.outputWidth = true;
                    }
                }
                else if ((opcode & 0b11111110) == 0b10101000) // TEST imm, ax
                {
                    instruction.type = DIS_TEST;
                    instruction.isWide = (opcode & 0b1);
                    instruction.operandCount = 2;
                    instruction.opDest = InitRegisterOperand(REG_AX);
                    LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, false);
                }
                else if ((opcode & 0b11111110) == 0b11110110)
                {
                    InstructionType types[] = {DIS_TEST, DIS_NOOP, DIS_NOT, DIS_NEG, DIS_MUL, DIS_IMUL, DIS_DIV, DIS_IDIV};

                    OperandByte operand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.type = types[operand.reg];
                    instruction.isWide = (opcode & 0b1);

                    if (operand.reg == 0b000)
                    {
                        instruction.operandCount = 2;
                        LoadMemoryOperand(file, &instruction.opDest, operand);
                        instruction.opDest.outputWidth = true;
                        LoadImmediateOperand(file, &instruction.opSrc, instruction.isWide, false);
                    }
                    else
                    {
                        instruction.operandCount = 1;
                        instruction.opDest.outputWidth = true;
                        LoadMemoryOperand(file, &instruction.opDest, operand);
                    }
                }
                else
                {
                    printf("; %x", opcode);
                }

                PrintInstruction(&instruction);

                if (execute)
                {
                    switch(instruction.type)
                    {
                        case DIS_MOV:
                        {

                            if (instruction.opDest.type == OP_REGISTER &&
                                instruction.opSrc.type == OP_IMMEDIATE)
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16 srcValue = instruction.opSrc.value;
                                    *((u16*)dest) = srcValue;
                                    printf("; %s := %d (0x%x)",
                                        registers16bit[instruction.opDest.regmemIndex],
                                        srcValue, srcValue);
                                }
                                else
                                {
                                    u8 srcValue = instruction.opSrc.valueLow;
                                    *((u8*)dest) = srcValue;
                                    printf("; %s := %d (0x%x)",
                                        registers8bit[instruction.opDest.regmemIndex],
                                        srcValue, srcValue);
                                }
                            }
                            else if ((instruction.opSrc.type == OP_REGISTER || instruction.opSrc.type == OP_SEGMENT_REGISTER) &&
                                (instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER))
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);
                                void* src = cpu.GetPointerToRegister(&instruction.opSrc, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16 newValue = *((u16*)src);
                                    *((u16*)dest) = newValue;

                                    printf("; %s := %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers16bit[instruction.opDest.regmemIndex]), 
                                        newValue, newValue);
                                }
                                else
                                {
                                    u8 newValue = *((u8*)src);
                                    *((u8*)dest) = newValue;
                                    printf("; %s := %d (0x%x)", 
                                        registers8bit[instruction.opDest.regmemIndex], 
                                        newValue, newValue);
                                }
                            }
                            else
                            {
                                printf(" ; MOV - not implemented");
                            }
                        }
                        break;
                        case DIS_ADD:
                        {
                            if ((instruction.opSrc.type == OP_REGISTER || instruction.opSrc.type == OP_SEGMENT_REGISTER) &&
                                (instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER))
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);
                                void* src  = cpu.GetPointerToRegister(&instruction.opSrc, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* src16  = (u16*)src;
                                    u16* dest16 = (u16*)dest;

                                    *dest16 += *src16;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers16bit[instruction.opDest.regmemIndex]), 
                                        *dest16, *dest16);
                                    
                                    printf(" | Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*dest16 == 0); 
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8 operand = *((u8*)src);
                                    u8* dest8 = ((u8*)dest);
                                    
                                    *dest8 += operand;
                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers8bit[instruction.opDest.regmemIndex]), 
                                        *dest8, *dest8);

                                    printf(" | Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*dest8 == 0);
                                    printf("->"); PrintFlags(cpu);
                                }

                            }
                            else if ((instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER)
                                && instruction.opSrc.type == OP_IMMEDIATE)
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* dest16 = (u16*)dest;

                                    *dest16 += instruction.opSrc.value;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers16bit[instruction.opDest.regmemIndex]), 
                                        *dest16, *dest16);
                                    
                                    printf(" | Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*dest16 == 0); 
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8* dest8 = ((u8*)dest);
                                    
                                    *dest8 += instruction.opSrc.valueLow;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers8bit[instruction.opDest.regmemIndex]), 
                                        *dest8, *dest8);

                                    printf(" | Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*dest8 == 0);
                                    printf("->"); PrintFlags(cpu);
                                }
                            }
                            else
                            {
                                printf(" ; ADD - not implemented");
                            }
                        }
                        break;
                        case DIS_SUB:
                        {
                            if ((instruction.opSrc.type == OP_REGISTER || instruction.opSrc.type == OP_SEGMENT_REGISTER) &&
                                (instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER))
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);
                                void* src  = cpu.GetPointerToRegister(&instruction.opSrc, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* src16  = (u16*)src;
                                    u16* dest16 = (u16*)dest;

                                    *dest16 -= *src16;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers16bit[instruction.opDest.regmemIndex]), 
                                        *dest16, *dest16);
                                    
                                    printf(" | Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*dest16 == 0); 
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8 operand = *((u8*)src);
                                    u8* dest8 = ((u8*)dest);
                                    
                                    *dest8 -= operand;
                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers8bit[instruction.opDest.regmemIndex]), 
                                        *dest8, *dest8);

                                    printf(" | Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*dest8 == 0);
                                    printf("->"); PrintFlags(cpu);
                                }
                            }
                            else if ((instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER)
                                && instruction.opSrc.type == OP_IMMEDIATE)
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* dest16 = (u16*)dest;

                                    *dest16 -= instruction.opSrc.value;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers16bit[instruction.opDest.regmemIndex]), 
                                        *dest16, *dest16);
                                    
                                    printf(" | Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*dest16 == 0); 
                                    cpu.parity = 0;
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8* dest8 = ((u8*)dest);
                                    
                                    *dest8 -= instruction.opSrc.valueLow;

                                    printf("; %s -> %d (0x%x)", 
                                        ((instruction.opDest.type == OP_SEGMENT_REGISTER) ? 
                                            registersSegment[instruction.opDest.regmemIndex] : 
                                            registers8bit[instruction.opDest.regmemIndex]), 
                                        *dest8, *dest8);

                                    printf(" | Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*dest8 == 0);
                                    cpu.parity = 0;
                                    printf("->"); PrintFlags(cpu);
                                }

                            }
                            else
                            {
                                printf(" ; SUB - not implemented");
                            }
                        }
                        break;
                        case DIS_CMP:
                        {
                            if ((instruction.opSrc.type == OP_REGISTER || instruction.opSrc.type == OP_SEGMENT_REGISTER) &&
                                (instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER))
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);
                                void* src  = cpu.GetPointerToRegister(&instruction.opSrc, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* src16  = (u16*)src;
                                    u16* dest16 = (u16*)dest;

                                    printf("; Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*src16 == *dest16);
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8* src8 = ((u8*)src);
                                    u8* dest8 = ((u8*)dest);
                                    
                                    printf("; Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*src8 == *dest8);
                                    printf("->"); PrintFlags(cpu);
                                }
                            }
                            else if ((instruction.opDest.type == OP_REGISTER || instruction.opDest.type == OP_SEGMENT_REGISTER)
                                && instruction.opSrc.type == OP_IMMEDIATE)
                            {
                                void* dest = cpu.GetPointerToRegister(&instruction.opDest, instruction.isWide);

                                if (instruction.isWide)
                                {
                                    u16* dest16 = (u16*)dest;
                                    
                                    printf("; Flags: "); PrintFlags(cpu); 
                                    cpu.sign = (*dest16 & 0x8000) >> 15;
                                    cpu.zero = (*dest16 == instruction.opSrc.value); 
                                    cpu.parity = 0;
                                    printf("->"); PrintFlags(cpu);
                                }
                                else
                                {
                                    u8* dest8 = ((u8*)dest);

                                    printf("; Flags: "); PrintFlags(cpu);
                                    cpu.sign = (*dest8 & 0x80) >> 7;
                                    cpu.zero = (*dest8 == instruction.opSrc.valueLow);
                                    cpu.parity = 0;
                                    printf("->"); PrintFlags(cpu);
                                }

                            }
                            else
                            {
                                printf(" ; CMP - not implemented");
                            }
                        }
                        break;
                        default:
                            printf(" ; not implemented");
                        break;
                    }
                }

                if (instruction.type != DIS_LOCK && instruction.type != DIS_REP)
                    printf("\n");
            }

            fclose(file);

            if (execute)
            {
                printf("\n");
                printf("; Final state:\n");
                printf("; AX: 0x%x (%d)\n", cpu.ax, cpu.ax);
                printf("; BX: 0x%x (%d)\n", cpu.bx, cpu.bx);
                printf("; CX: 0x%x (%d)\n", cpu.cx, cpu.cx);
                printf("; DX: 0x%x (%d)\n", cpu.dx, cpu.dx);
                printf("; SP: 0x%x (%d)\n", cpu.sp, cpu.sp);
                printf("; BP: 0x%x (%d)\n", cpu.bp, cpu.bp);
                printf("; SI: 0x%x (%d)\n", cpu.si, cpu.si);
                printf("; DI: 0x%x (%d)\n", cpu.di, cpu.di);
                printf("\n");
                printf("; ES: 0x%x (%d)\n", cpu.es, cpu.es);
                printf("; CS: 0x%x (%d)\n", cpu.cs, cpu.cs);
                printf("; SS: 0x%x (%d)\n", cpu.ss, cpu.ss);
                printf("; DS: 0x%x (%d)\n", cpu.ds, cpu.ds);
                printf("\n");
                printf("; Flags: "); PrintFlags(cpu); printf("\n");
            }
        }
        else
        {
            printf("Failed to open file: %s\n", fileName);
        }
    }
    else
    {
        printf("Usage: main.exe [-e] <filename>\n");
        printf("    -e -- Execute\n");
    }
}