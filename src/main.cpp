#include <stdio.h>
#include <stdint.h>

#include "common.cpp"
#include "disassembly.cpp"

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


Disassembly_Operand InitRegisterOperand(RMField registerIndex)
{
    Disassembly_Operand result;
    result.type = OP_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Disassembly_Operand InitSegmentRegisterOperand(RMField registerIndex)
{
    Disassembly_Operand result;
    result.type = OP_SEGMENT_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Disassembly_Operand InitImmediateOperand(i16 value)
{
    Disassembly_Operand result;
    result.type = OP_IMMEDIATE;
    result.value = value;
    return result;
}

/// @brief Loads memory 
/// @param file handle to assembled file
/// @param[out] operand output location for operand data
void LoadMemoryOperand(FILE* file, Disassembly_Operand* operand, ModField mode, RMField regmemField)
{
    // TODO: This function changes the operand type for registers.
    // Maybe it should do the same with memory operands.

    operand->type = OP_MEMORY;
    operand->modField = mode;
    operand->regmemIndex = regmemField;

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
void LoadImmediateOperand(FILE* file, Disassembly_Operand* operand, bool wideOperation, bool signExtend)
{
    operand->type = OP_IMMEDIATE;

    if (signExtend)
    {
        operand->valueLow = (i8)Load8BitValue(file);
        if ((operand->valueLow >> 7) & 0b1)
        {
            operand->valueHigh = 0b11111111;
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

static char* const registers8bit[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
static char* const registers16bit[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
static char* const registersSegment[] = {"es", "cs", "ss", "ds"};
static char* const effectiveAddressTable[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

void PrintOperand(Disassembly_Operand operand, bool wideOperation)
{
    // STUDY: Check if register mode ever needs to have the width output

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
            printf("%d", (wideOperation? operand.value : operand.valueLow));
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
                    printf("[%d]", operand.value);
                }
                else
                {
                    printf("[%s]", effectiveAddressTable[operand.regmemIndex]);
                }
            }
            else if (operand.modField == MEMORY_8BIT_MODE)
            {
                PrintAddressOperand(effectiveAddressTable[operand.regmemIndex], operand.valueLow);
            }
            else if (operand.modField == MEMORY_16BIT_MODE)
            {
                PrintAddressOperand(effectiveAddressTable[operand.regmemIndex], operand.value);
            }
            else
            {
                printf("; error: memory operand in register mode\n");
            }
        }
        break;
    }
}

void PrintOperands(Disassembly_Operand operand1, Disassembly_Operand operand2, bool wide)
{
    PrintOperand(operand1, wide);
    printf(", ");
    PrintOperand(operand2, wide);
}

#define MASK_INST_1BYTE_REG 0b11111000
enum Inst_1ByteRegisterInstructions
{
    INST_INC_REG           = 0b01000000,
    INST_DEC_REG           = 0b01001000,
    INST_PUSH_REG          = 0b01010000,
    INST_POP_REG           = 0b01011000,
    INST_XCHG_ACC_WITH_REG = 0b10010000,
    INST_MOV_IMM_TO_REG    = 0b10110000,
    INST_MOV_IMM_TO_REG_W  = 0b10111000,
};

enum Inst_1Byte
{
    INST_DAA   = 0b00100111,
    INST_DAS   = 0b00101111,
    INST_AAA   = 0b00110111,
    INST_AAS   = 0b00111111,

    INST_CBW   = 0b10011000,
    INST_CWD   = 0b10011001,

    INST_PUSHF = 0b10011100,
    INST_POPF  = 0b10011101,
    INST_SAHF  = 0b10011110,
    INST_LAHF  = 0b10011111,

    INST_XLAT  = 0b11010111,

    INST_INT3  = 0b11001100, // Interrupt 3
    INST_INTO  = 0b11001110, // Interrupt on overflow
    INST_IRET  = 0b11001111, // Interrupt return

    INST_CLC   = 0b11111000, // Clear carry
    INST_CMC   = 0b11110101, // Complement carry
    INST_STC   = 0b11111001, // Set carry
    INST_CLD   = 0b11111100, // Clear direction
    INST_STD   = 0b11111101, // Set direction
    INST_CLI   = 0b11111010, // Clear interrupt
    INST_STI   = 0b11111011, // Set interrupt
    INST_HLT   = 0b11110100, // Halt
    INST_WAIT  = 0b10011011, // Wait
    INST_LOCK  = 0b11110000, // Bus lock prefix


    // TODO: Find instruction names and definitions
    INST_RET_WITHIN_SEGMENT   = 0b11000011,
    INST_RET_INTERSEGMENT  = 0b11001011,


    //

    INST_LEA = 0b10001101,
    INST_LDS = 0b11000101,
    INST_LES = 0b11000100,
};

int main(int argc, char** argv)
{
    char* subOpType[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};

    Disassembly_InstructionType instructionSubtypes[] = {
        DIS_ADD, DIS_OR, DIS_ADC, DIS_SBB, DIS_AND, DIS_SUB, DIS_XOR, DIS_CMP
    };

    if (argc == 2)
    {
        char* fileName = argv[1];
        printf("; Disassembly: %s\n", fileName);
        printf("bits 16\n");

        FILE* file = fopen(fileName, "rb");

        if (file)
        {
            int input;
            // NOTE: fgetc is important here otherwise the loop might not end.
            while((input = fgetc(file)) != EOF)
            {
                u8 opcode = (u8)input;

                Disassembly_Instruction instruction {0};

                if (opcode == INST_XLAT) instruction.type = DIS_XLAT;
                else if (opcode == INST_DAA) instruction.type = DIS_DAA;
                else if (opcode == INST_AAA) instruction.type = DIS_AAA;
                else if (opcode == INST_AAS) instruction.type = DIS_AAS;
                else if (opcode == INST_DAS) instruction.type = DIS_DAS;
                else if (opcode == INST_CBW) instruction.type = DIS_CBW;
                else if (opcode == INST_CWD) instruction.type = DIS_CWD;
                else if (opcode == INST_INT3) 
                {
                    instruction.type = DIS_INT;
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand(3);
                }
                else if (opcode == INST_INTO) instruction.type = DIS_INTO;
                else if (opcode == INST_IRET) instruction.type = DIS_IRET;
                else if (opcode == INST_CLC) instruction.type = DIS_CLC;
                else if (opcode == INST_CMC) instruction.type = DIS_CMC;
                else if (opcode == INST_STC) instruction.type = DIS_STC;
                else if (opcode == INST_CLD) instruction.type = DIS_CLD;
                else if (opcode == INST_STD) instruction.type = DIS_STD;
                else if (opcode == INST_CLI) instruction.type = DIS_CLI;
                else if (opcode == INST_STI) instruction.type = DIS_STI;
                else if (opcode == INST_HLT) instruction.type = DIS_HLT;
                else if (opcode == INST_WAIT) instruction.type = DIS_WAIT;
                else if (opcode == INST_PUSHF) instruction.type = DIS_PUSHF;
                else if (opcode == INST_POPF) instruction.type = DIS_POPF;
                else if (opcode == INST_SAHF) instruction.type = DIS_SAHF;
                else if (opcode == INST_LAHF) instruction.type = DIS_LAHF;
                else if (opcode == INST_LOCK)
                {
                    instruction.type = DIS_LOCK;
                }
                // FIXME: Probably inaccurate ret instructions
                else if (opcode == INST_RET_INTERSEGMENT) instruction.type = DIS_RET; //printf("ret ; intersegment");
                else if (opcode == INST_RET_WITHIN_SEGMENT) instruction.type = DIS_RET; // printf("ret ; within segment");
                else if (opcode == INST_LEA || opcode == INST_LDS || opcode == INST_LES)
                {
                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    if (opcode == INST_LEA)
                        instruction.type = DIS_LEA;
                    else if (opcode == INST_LDS)
                        instruction.type = DIS_LDS;
                    else
                        instruction.type = DIS_LES;

                    instruction.isWide = true;
                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.operand2, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & 0b11000100) == 0b00000000)
                {
                    instruction.isWide = (opcode & 0b1);
                    instruction.switchOperands = !((opcode >> 1) & 0b1);
                    instruction.type = instructionSubtypes[((opcode >> 3) & 0b111)];
                    instruction.operandCount = 2;

                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operand1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.operand2, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & 0b11110000) == 0b01010000) // Push/pop register
                {
                    bool isPop = ((opcode >> 3) & 0b1);
                    RMField reg = (RMField)(opcode & 0b111);

                    instruction.type = (isPop? DIS_POP : DIS_PUSH);
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.operand1 = InitRegisterOperand(reg);
                }
                else if ((opcode & 0b11100110) == 0b00000110) // Push/pop segment register
                {
                    bool isPop = (opcode & 0b1);
                    u8 segreg = ((opcode >> 3) & 0b11);

                    instruction.type = (isPop? DIS_POP : DIS_PUSH);
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.operand1 = InitSegmentRegisterOperand((RMField)segreg);
                }
                else if ((opcode & 0b11110100) == 0b11100100) // IN/OUT fixed port
                {
                    bool variableBit = ((opcode >> 3) & 0b1);
                    bool typeBit = ((opcode >> 1) & 0b1);

                    instruction.type = (typeBit? DIS_OUT : DIS_IN);
                    instruction.switchOperands = (typeBit);
                    instruction.isWide = (opcode & 0b1);

                    // FIXME: Operands should be unsigned
                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(REG_AX);

                    if (variableBit)
                    {
                        instruction.operand2 = InitRegisterOperand(REG_DX);
                    }
                    else
                    {
                        // FIXME: Print as wide when (variableBit | wideBit)
                        LoadImmediateOperand(file, &instruction.operand2, false, false);
                    }
                }
                else if ((opcode & 0b11111100) == (0b10000100))
                {
                    instruction.isWide = (opcode & 0b1);
                    instruction.type = DIS_XCHG;

                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.operand2, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_XCHG_ACC_WITH_REG)
                {
                    instruction.type = DIS_XCHG;
                    instruction.operandCount = 2;
                    instruction.isWide = true;
                    instruction.operand1 = InitRegisterOperand(REG_AX);
                    instruction.operand2 = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_INC_REG)
                {
                    instruction.type = DIS_INC;
                    instruction.isWide = true;
                    instruction.operandCount = 1;
                    instruction.operand1 = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_DEC_REG)
                {
                    instruction.type = DIS_DEC;
                    instruction.isWide = true;
                    instruction.operandCount = 1;
                    instruction.operand1 = InitRegisterOperand((RMField)(opcode & 0b111));
                }
                else if ((opcode & 0b11000100) == 0b00000100) // Immediate to accumulator
                {
                    instruction.isWide = (opcode & 0b1);
                    u8 instructionType = ((opcode >> 3) & 0b111);
                    instruction.type = instructionSubtypes[instructionType];

                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(REG_AX); // Same ID as REG_AL
                    LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, false);
                }
                else if ((opcode & 0b11111100) == 0b10000000) // Immediate to register/memory
                {
                    bool signBit = ((opcode >> 1) & 0b1);
                    instruction.isWide = (opcode & 0b1);

                    Inst_Operand instructionOperand = Inst_ParseOperand(Load8BitValue(file));

                    // NOTE: For immediate instructions, the REG part of the operand byte determines the operation type.
                    instruction.type = instructionSubtypes[instructionOperand.reg];

                    instruction.operandCount = 2;
                    instruction.operand1.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.operand1, instructionOperand.mod, instructionOperand.rm);
                    LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, signBit);
                }
                else if ((opcode & 0b11111110) == 0b11000110) // MOVE immediate to register/memory
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = (opcode & 0b1);

                    Inst_Operand instOperand = Inst_ParseOperand((u8)Load8BitValue(file));

                    Assert(instOperand.reg == 0b000);// NOTE: Other reg values are not used. 

                    instruction.operandCount = 2;
                    instruction.operand2.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.operand1, instOperand.mod, instOperand.rm);
                    LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, false);
                }
                else if ((opcode & 0b11111100) == 0b10001000)
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = (opcode & 0b1);
                    instruction.switchOperands = !((opcode >> 1) & 0b1);

                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, &instruction.operand2, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & 0b11111100) == 0b10100000) // MOV accumulator-memory
                {
                    // NOTE: The direction bit here has opposite meaning to the standard one, and it's written as 2 separate commands in the manual.
                    instruction.switchOperands = ((opcode >> 1) & 0b1);
                    instruction.isWide = (opcode & 0b1);

                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(REG_AX);
                    instruction.operand2.type = OP_MEMORY;
                    instruction.operand2.regmemIndex = MEM_DIRECT;
                    instruction.operand2.value = (i16)Load16BitValue(file);
                }
                else if (opcode == 0b11000010) // RET - within seg adding immediate to SP
                {
                    instruction.type = DIS_RET;
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand(Load16BitValue(file));
                }
                else if ((opcode & 0b11110000) == 0b10110000) // MOV imm -> reg
                {
                    instruction.type = DIS_MOV;
                    instruction.isWide = ((opcode >> 3) & 0b1);
                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand((RMField)(opcode & 0b111));
                    LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, false);
                }
                else if ((opcode & 0b11110000) == 0b01110000)
                {
                    Disassembly_InstructionType jumpSuptypes[] = {
                        DIS_JO, DIS_JNO, DIS_JB, DIS_JNB, DIS_JE, DIS_JNE, DIS_JBE, DIS_JNBE,
                        DIS_JS, DIS_JNS, DIS_JP, DIS_JNP, DIS_JL, DIS_JNL, DIS_JLE, DIS_JNLE
                    };
                    // TODO: Jump labels

                    u8 jumpType = (opcode & 0b1111);

                    instruction.type = jumpSuptypes[jumpType];
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand(((i8)Load8BitValue(file) + 2));
                    instruction.operand1.outputWidth = false;

                    /*
                    if (displacement >= 0)
                    {
                        printf(" $+%d", displacement);
                    }
                    else
                    {
                        printf(" $%d", displacement);
                    }*/
                }
                else if ((opcode & 0b11111100) == 0b11100000)
                {
                    Disassembly_InstructionType loopTypes[] = { DIS_LOOPNZ, DIS_LOOPZ, DIS_LOOP, DIS_JCXZ};

                    instruction.type = loopTypes[(opcode & 0b11)];
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand((i8)Load8BitValue(file) + 2);
                    instruction.operand1.outputWidth = false;
                }
                else if ((opcode & 0b11111111) == 0b10001111) // pop
                {
                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));
                    Assert(instOperand.reg == 0b000); // NOTE: Other values are not used.
                    
                    instruction.type = DIS_POP;
                    instruction.operandCount = 1;
                    instruction.isWide = true;
                    instruction.operand1.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.operand1, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & 0b11111110) == 0b11111110)
                {
                    Disassembly_InstructionType instructionSubtypesInc[] = {DIS_INC, DIS_DEC, DIS_CALL, DIS_CALL, DIS_JMP, DIS_JMP, DIS_PUSH};

                    Inst_Operand instructionOperand = Inst_ParseOperand(Load8BitValue(file));

                    Assert(instructionOperand.reg != 0b111);

                    instruction.isWide = (opcode & 0b1);
                    instruction.type = instructionSubtypesInc[instructionOperand.reg];

                    instruction.operandCount = 1;
                    LoadMemoryOperand(file, &instruction.operand1, instructionOperand.mod, instructionOperand.rm);

                    if (instructionOperand.reg == 0b110 || 
                        instructionOperand.reg == 0b000 || 
                        instructionOperand.reg == 0b001) // push/inc/dec
                    {
                        instruction.operand1.outputWidth = true;
                    }
                }
                else
                {
                    printf("; %x", opcode);
                }

                Assert(instruction.operandCount >= 0 && instruction.operandCount <= 2);

                // Print instruction
                printf(operationNames[instruction.type]);
                printf(" ");
                if (instruction.operandCount == 1)
                    PrintOperand(instruction.operand1, instruction.isWide);
                else if (instruction.operandCount == 2)
                {
                    if (instruction.switchOperands)
                        PrintOperands(instruction.operand2, instruction.operand1, instruction.isWide);
                    else
                        PrintOperands(instruction.operand1, instruction.operand2, instruction.isWide);
                }

                if (instruction.type != DIS_LOCK)
                    printf("\n");
            }

            fclose(file);
        }
        else
        {
            printf("Failed to open file: %s\n", fileName);
        }
    }
    else
    {
        printf("Usage: main.exe <filename>\n");
    }
}