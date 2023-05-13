#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.cpp"
#include "disassembly.cpp"

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
    INST_INT   = 0b11001101, // Interrupt (specified)
    INST_INTO  = 0b11001110, // Interrupt on overflow
    INST_IRET  = 0b11001111, // Interrupt return

    INST_AAM   = 0b11010100,
    INST_AAD   = 0b11010101,

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


    // TODO: Find definitions
    INST_RET_WITHIN_SEGMENT   = 0b11000011,
    INST_RET_INTERSEGMENT  = 0b11001011,

    //

    INST_LEA = 0b10001101,
    INST_LDS = 0b11000101,
    INST_LES = 0b11000100,

    // MOV with segment registers
    INST_MOV_REGMEM_SR = 0b10001110,
    INST_MOV_SR_REGMEM = 0b10001100
};

void PrintInstruction(Disassembly_Instruction* inst)
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
            i8 displacement = inst->operand1.valueLow;
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
            PrintOperand(inst->operand1, inst->isWide);
            printf(", ");
            PrintOperand(inst->operand2, false);
        }
        break;

        case DIS_IN:
        {
            PrintOperand(inst->operand1, inst->isWide);
            printf(", ");
            PrintOperand(inst->operand2, true);
        } 
        break;
        case DIS_OUT:
        {
            PrintOperand(inst->operand2, true);
            printf(", ");
            PrintOperand(inst->operand1, inst->isWide);
        }
        break;

        default:
        {
            if (inst->operandCount == 1)
            {
                PrintOperand(inst->operand1, inst->isWide);
            }
            else if (inst->operandCount == 2)
            {
                if (inst->switchOperands)
                {
                    PrintOperand(inst->operand2, inst->isWide);
                    printf(", ");
                    PrintOperand(inst->operand1, inst->isWide);
                }
                else
                {
                    PrintOperand(inst->operand1, inst->isWide);
                    printf(", ");
                    PrintOperand(inst->operand2, inst->isWide);
                }
            }

        }
        break;
    }
}

struct CPU 
{
    // Registers

    union
    {
        u16 reg16[8];
        struct { u16 ax, cx, dx, bx, sp, bp, si, di; };

        u8  reg8[16];
        struct { u8 al, ah, cl, ch, dl, dh, bl, bh; };
    };

    // Segment registers
    union
    {
        u16 regseg[4];
        struct { u16 es, cs, ss, ds; };
    };

    void* GetPointerToRegister(Disassembly_Operand* op, bool wide)
    {
        Assert(op->type == OP_REGISTER || op->type == OP_SEGMENT_REGISTER);

        if (op->type == OP_REGISTER)
        {
            Assert((int)op->regmemIndex < 8)

            if (wide) 
                return &reg16[op->regmemIndex];

            switch (op->regmemIndex)
            {
                case REG_AL: return &al;
                case REG_AH: return &ah;
                case REG_CL: return &cl;
                case REG_CH: return &ch;
                case REG_DL: return &dl;
                case REG_DH: return &dh;
                case REG_BL: return &bl;
                case REG_BH: return &bh;
                default:
                    Assert(false);
                    return nullptr;
            }
        }
        else if (op->type == OP_SEGMENT_REGISTER)
        {
            Assert((int)op->regmemIndex < 4);

            return &regseg[op->regmemIndex];
        }
        else
        {
            return nullptr;
        }
    }
};

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

int main(int argc, char** argv)
{
    Disassembly_InstructionType instructionSubtypes[] = {
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
            else
            {
                // TODO: Error logging?
            }
        }
    }

    if (argc >= 2)
    {
        char* fileName = argv[argc - 1];

        FILE* file = fopen(fileName, "rb");

        if (file)
        {
            printf("; Disassembly: %s\n", fileName);
            printf("bits 16\n");

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
                else if (opcode == INST_INT)
                {
                    instruction.type = DIS_INT;
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand(Load8BitValue(file));
                }
                else if (opcode == INST_MOV_REGMEM_SR || opcode == INST_MOV_SR_REGMEM)
                {
                    Inst_Operand operand = Inst_ParseOperand(Load8BitValue(file));
                    instruction.type = DIS_MOV;
                    instruction.operandCount = 2;
                    instruction.isWide = true;

                    bool toSegmentRegister = ((opcode >> 1) & 0b1);

                    Disassembly_Operand* segment;
                    Disassembly_Operand* regmem;
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
                    LoadMemoryOperand(file, regmem, operand.mod, operand.rm);
                }
                else if ((opcode & 0b11111110) == 0b11110010) instruction.type = DIS_REP;
                else if ((opcode & 0b11111110) == 0b10100100) instruction.type = (opcode & 0b1) ? DIS_MOVSW : DIS_MOVSB;
                else if ((opcode & 0b11111110) == 0b10100110) instruction.type = (opcode & 0b1) ? DIS_CMPSW : DIS_CMPSB;
                else if ((opcode & 0b11111110) == 0b10101110) instruction.type = (opcode & 0b1) ? DIS_SCASW : DIS_SCASB;
                else if ((opcode & 0b11111110) == 0b10101100) instruction.type = (opcode & 0b1) ? DIS_LODSW : DIS_LODSB;
                else if ((opcode & 0b11111110) == 0b10101010) instruction.type = (opcode & 0b1) ? DIS_STOSW : DIS_STOSB;
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
                        LoadImmediateOperand(file, &instruction.operand2, false, false);
                    }
                }
                else if ((opcode & 0b11111100) == (0b10000100))
                {
                    instruction.isWide = (opcode & 0b1);
                    instruction.type = ((opcode >> 1) & 0b1) ? DIS_XCHG : DIS_TEST;

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
                else if ((opcode & 0b11111100) == 0b11010000) // Logic operations
                {
                    Disassembly_InstructionType logicSubtypes[] = {DIS_ROL, DIS_ROR, DIS_RCL, DIS_RCR, DIS_SHL, DIS_SHR, DIS_NOOP, DIS_SAR};
                    Inst_Operand operand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.type = logicSubtypes[operand.reg];
                    instruction.isWide = (opcode & 0b1);

                    instruction.operandCount = 2;
                    instruction.operand1.outputWidth = true;
                    LoadMemoryOperand(file, &instruction.operand1, operand.mod, operand.rm);

                    bool shiftByCLValue = ((opcode >> 1) & 0b1);
                    if (shiftByCLValue)
                    {
                        instruction.operand2 = InitRegisterOperand(REG_CX);
                    }
                    else
                    {
                        instruction.operand2 = InitImmediateOperand(1);
                    }
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
                    bool switchOperands = ((opcode >> 1) & 0b1);

                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.operandCount = 2;

                    Disassembly_Operand* op1;
                    Disassembly_Operand* op2;
                    if (switchOperands)
                    {
                        op1 = &instruction.operand1;
                        op2 = &instruction.operand2;
                    }
                    else
                    {
                        op1 = &instruction.operand2;
                        op2 = &instruction.operand1;
                    }
                    *op1 = InitRegisterOperand(instOperand.reg);
                    LoadMemoryOperand(file, op2, instOperand.mod, instOperand.rm);
                }
                else if ((opcode & 0b11111100) == 0b10100000) // MOV accumulator-memory
                {
                    // NOTE: The direction bit here has opposite meaning to the standard one, and it's written as 2 separate commands in the manual.
                    instruction.type = DIS_MOV;
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
                    instruction.operand1 = InitImmediateOperand((i16)Load16BitValue(file));
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
                }
                else if ((opcode & 0b11111100) == 0b11100000)
                {
                    Disassembly_InstructionType loopTypes[] = { DIS_LOOPNZ, DIS_LOOPZ, DIS_LOOP, DIS_JCXZ};

                    instruction.type = loopTypes[(opcode & 0b11)];
                    instruction.operandCount = 1;
                    instruction.operand1 = InitImmediateOperand((i8)Load8BitValue(file) + 2);
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
                else if ((opcode & 0b11111110) == 0b10101000) // TEST imm, ax
                {
                    instruction.type = DIS_TEST;
                    instruction.isWide = (opcode & 0b1);
                    instruction.operandCount = 2;
                    instruction.operand1 = InitRegisterOperand(REG_AX);
                    LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, false);
                }
                else if ((opcode & 0b11111110) == 0b11110110)
                {
                    Disassembly_InstructionType types[] = {DIS_TEST, DIS_NOOP, DIS_NOT, DIS_NEG, DIS_MUL, DIS_IMUL, DIS_DIV, DIS_IDIV};

                    Inst_Operand operand = Inst_ParseOperand(Load8BitValue(file));

                    instruction.type = types[operand.reg];
                    instruction.isWide = (opcode & 0b1);

                    if (operand.reg == 0b000)
                    {
                        instruction.operandCount = 2;
                        LoadMemoryOperand(file, &instruction.operand1, operand.mod, operand.rm);
                        instruction.operand1.outputWidth = true;
                        LoadImmediateOperand(file, &instruction.operand2, instruction.isWide, false);
                    }
                    else
                    {
                        instruction.operandCount = 1;
                        instruction.operand1.outputWidth = true;
                        LoadMemoryOperand(file, &instruction.operand1, operand.mod, operand.rm);
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