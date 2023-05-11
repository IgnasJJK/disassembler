#include <stdio.h>
#include <stdint.h>

#define Assert(expr) if (!(expr)) { *(int*)0 = 100; }

typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t i8;
typedef int16_t i16;

#define OP_MASK 0b00111000 //< Mask for operator type in first byte

#define MOV_RM 0b10001000

#define ADD_RM 0b00000000
#define SUB_RM 0b00101000 // 001010dw
#define CMP_RM 0b00111000 // 001110dw

#define MOV_IMM_REG 0b10110000
#define MOV_IMM_MEM 0b11000110
#define MOV_AX_MEMORY 0b10100000

// ADD, CMP, SUB (immediate)
#define ARITHMETIC_IMM 0b10000000 // 100000sw

// Immediate to accumulator
#define ACCUM_IMM_OP 0b00000100 // NOTE: Includes OP_MASK (00 OPC 100)

// REG/MEM Operation (ADD, CMP, SUB)
#define REG_RM_OP 0b00000000 // NOTE: Includes OP_MASK (00 OPC 000)

enum ModField
{
    MEMORY_0BIT_MODE,
    MEMORY_8BIT_MODE,
    MEMORY_16BIT_MODE,
    REGISTER_MODE
};
enum RMField
{
    // Table columns:
    // - Register mode, 8-bit
    // - Register mode, 16-bit (wide)
    // - Memory mode, effective address

    // NOTE: (disp) is omitted when mod = 00

    MEM_BX_SI, // | AL | AX | BX + SI + (disp)
    MEM_BX_DI, // | CL | CX | BX + DI + (disp)
    MEM_BP_SI, // | DL | DX | BP + SI + (disp)
    MEM_BP_DI, // | BL | BX | BP + DI + (disp)
    MEM_SI,    // | AH | SP | SI + (disp)
    MEM_DI,    // | CH | BP | DI + (disp)
    MEM_BP,    // | DH | SI | BP + (disp) // NOTE: (If MOD == 110, then DISP is direct address)
    MEM_BX,    // | BH | DI | BX + (disp)

#define REG_AX MEM_BX_SI
#define REG_BX MEM_BP_DI
#define REG_CX MEM_BX_DI
#define REG_DX MEM_BP_SI

#define MEM_DIRECT MEM_BP
};

struct Inst_Operand
{
    ModField mod; // Operation mode. May correspond to the number of displacement bytes.
    RMField reg;
    RMField rm;
};

Inst_Operand Inst_ParseOperand(u8 byte)
{
    // Byte structure:
    // - mod: (2 bit) mode 
    // - reg: (3 bit) register
    // - r/m: (3 bit) register/memory

    Inst_Operand result;
    result.mod = (ModField)((byte >> 6) & 0b11);
    result.reg = (RMField)((byte >> 3) & 0b111);
    result.rm = (RMField)(byte & 0b111);

    return result;
}

enum Disassembly_OperandType
{
    OP_IMMEDIATE,
    OP_MEMORY,
    OP_REGISTER
};

struct Disassembly_Operand
{
    Disassembly_OperandType type;

    RMField regmemIndex;
    ModField modField;

    bool outputWidth;

    union
    {
        i16 value;
        struct
        {
            i8 valueLow; // NOTE: 8-bit operand is stored in low byte
            i8 valueHigh;
        };
    };

    inline void InitRegister(RMField registerIndex)
    {
        type = OP_REGISTER;
        regmemIndex = registerIndex;
    }
};

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
};


int main(int argc, char** argv)
{
    char* registers8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    char* registers16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    char* effectiveAddress[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

    char* subOpType[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};

    if (argc == 2)
    {
        char* fileName = argv[1];
        printf("; Disassembly: %s\n", fileName);
        printf("bits 16\n");

        FILE* file = fopen(fileName, "rb");

        if (file)
        {
            int input;
            while((input = fgetc(file)) != EOF)
            {
                u8 opcode = (u8)input;

                if (opcode == INST_XLAT) printf("xlat");
                else if (opcode == INST_DAA) printf("daa");
                else if (opcode == INST_AAA) printf("aaa");
                else if (opcode == INST_AAS) printf("aas");
                else if (opcode == INST_DAS) printf("das");
                else if (opcode == INST_CBW) printf("cbw");
                else if (opcode == INST_CWD) printf("cwd");
                else if (opcode == INST_INT3) printf("int 3");
                else if (opcode == INST_INTO) printf("into");
                else if (opcode == INST_IRET) printf("iret");
                else if (opcode == INST_CLC) printf("clc");
                else if (opcode == INST_CMC) printf("cmc");
                else if (opcode == INST_STC) printf("stc");
                else if (opcode == INST_CLD) printf("cld");
                else if (opcode == INST_STD) printf("std");
                else if (opcode == INST_CLI) printf("cli");
                else if (opcode == INST_STI) printf("sti");
                else if (opcode == INST_HLT) printf("hlt");
                else if (opcode == INST_WAIT) printf("wait");
                else if (opcode == INST_LOCK)
                {
                    printf("lock "); 
                    continue;
                }
                // FIXME: Probably inaccurate ret instructions
                else if (opcode == INST_RET_INTERSEGMENT) printf("ret ; intersegment");
                else if (opcode == INST_RET_WITHIN_SEGMENT) printf("ret ; within segment");
                #define INST_LEA 0b10001101
                #define INST_LDS 0b11000101
                #define INST_LES 0b11000100
                else if (opcode == INST_LEA || opcode == INST_LDS || opcode == INST_LES)
                {
                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    Disassembly_Operand opDest {0};
                    opDest.InitRegister(instOperand.reg);

                    Disassembly_Operand opSrc {0};
                    LoadMemoryOperand(file, &opSrc, instOperand.mod, instOperand.rm);

                    if (opcode == INST_LEA)
                        printf("lea ");
                    else if (opcode == INST_LDS)
                        printf("lds ");
                    else
                        printf("les ");

                    PrintOperands(opDest, opSrc, true);
                }
                else if ((opcode & 0b11000100) == REG_RM_OP)
                {
                    bool directionBit = ((opcode >> 1) & 0b1);
                    bool wideBit = (opcode & 0b1);
                    u8 instructionType = ((opcode >> 3) & 0b111);

                    printf("%s ", subOpType[instructionType]);

                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    Disassembly_Operand operand1{0};
                    operand1.InitRegister(instOperand.reg);

                    Disassembly_Operand operand2{0};
                    LoadMemoryOperand(file, &operand2, instOperand.mod, instOperand.rm);

                    if (directionBit)
                    {
                        PrintOperands(operand1, operand2, wideBit);
                    }
                    else
                    {
                        PrintOperands(operand2, operand1, wideBit);
                    }
                }
                else if ((opcode & 0b11110000) == 0b01010000) // Push/pop register
                {
                    bool isPop = ((opcode >> 3) & 0b1);
                    RMField reg = (RMField)(opcode & 0b111);
                    printf("%s %s", (isPop? "pop" : "push"), registers16bit[reg]);
                }
                else if ((opcode & 0b11100110) == 0b00000110) // Push/pop segment register
                {
                    bool isPop = (opcode & 0b1);
                    u8 segreg = ((opcode >> 3) & 0b11);

                    printf("%s %s", (isPop? "pop" : "push"), registersSegment[segreg]);
                }
                else if ((opcode & 0b11110100) == 0b11100100) // IN/OUT fixed port
                {
                    bool variableBit = ((opcode >> 3) & 0b1);
                    bool typeBit = ((opcode >> 1) & 0b1);
                    bool wideBit = (opcode & 0b1);

                    Disassembly_Operand acc {0};
                    acc.InitRegister(REG_AX);

                    Disassembly_Operand port {0};
                    if (variableBit)
                    {
                        port.InitRegister(REG_DX);
                    }
                    else
                    {
                        LoadImmediateOperand(file, &port, false, false);
                    }

                    // FIXME: Operands should be unsigned
                    if (typeBit)
                    {
                        printf("out ");
                        PrintOperand(port, (variableBit | wideBit));
                        printf(", ");
                        PrintOperand(acc, wideBit);
                    }
                    else
                    {
                        printf("in ");
                        PrintOperand(acc, wideBit);
                        printf(", ");
                        PrintOperand(port, (variableBit | wideBit));
                    }
                }
                else if ((opcode & 0b11111100) == (0b10000100))
                {
                    bool wideBit = (opcode & 0b1);
                    Inst_Operand instOperand = Inst_ParseOperand(Load8BitValue(file));

                    Disassembly_Operand operand1 {0};
                    operand1.InitRegister(instOperand.reg);

                    Disassembly_Operand operand2 {0};
                    LoadMemoryOperand(file, &operand2, instOperand.mod, instOperand.rm);

                    printf("xchg ");
                    PrintOperands(operand1, operand2, wideBit);
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_XCHG_ACC_WITH_REG)
                {
                    Disassembly_Operand opAccumulator {0};
                    opAccumulator.InitRegister(REG_AX);

                    Disassembly_Operand opRegister {0};
                    opRegister.InitRegister((RMField)(opcode & 0b111));

                    printf("xchg ");
                    PrintOperands(opAccumulator, opRegister, true);
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_INC_REG)
                {
                    Disassembly_Operand opRegister {0};
                    opRegister.InitRegister((RMField)(opcode & 0b111));

                    printf("inc ");
                    PrintOperand(opRegister, true);
                }
                else if ((opcode & MASK_INST_1BYTE_REG) == INST_DEC_REG)
                {
                    Disassembly_Operand opRegister {0};
                    opRegister.InitRegister((RMField)(opcode & 0b111));

                    printf("dec ");
                    PrintOperand(opRegister, true);
                }
                else if ((opcode & 0b11000100) == 0b00000100) // Immediate to accumulator
                {
                    u8 instructionType = ((opcode >> 3) & 0b111);
                    bool wideBit = (opcode & 0b1);

                    printf("%s ", subOpType[instructionType]);

                    Disassembly_Operand operandDest {0};
                    operandDest.InitRegister(REG_AX); // Same ID as REG_AL

                    Disassembly_Operand operandSrc {0};
                    LoadImmediateOperand(file, &operandSrc, wideBit, false);

                    PrintOperands(operandDest, operandSrc, wideBit);
                }
                else if ((opcode & 0b11111100) == 0b10000000) // Immediate to register/memory
                {
                    // Structure:
                    //   1000 00sw
                    //   [mod(2) op(3) rm(3)]
                    //   [DISP-LO]  [DISP-HI]
                    //   [data]     [data if s:w = 01]
                    
                    bool signBit = ((opcode >> 1) & 0b1);
                    bool wideBit = (opcode & 0b1);

                    Inst_Operand instructionOperand = Inst_ParseOperand(Load8BitValue(file));

                    // NOTE: For immediate instructions, the REG part of the operand byte determines the operation type.
                    printf("%s ", subOpType[instructionOperand.reg]);

                    Disassembly_Operand operandDest {0};
                    operandDest.outputWidth = true;
                    LoadMemoryOperand(file, &operandDest, instructionOperand.mod, instructionOperand.rm);

                    Disassembly_Operand operandSrc {0};
                    LoadImmediateOperand(file, &operandSrc, wideBit, signBit);
                    
                    PrintOperands(operandDest, operandSrc, wideBit);
                }
                else if ((opcode & 0b11111110) == MOV_IMM_MEM) // MOVE immediate to register/memory
                {
                    printf("mov ");
                    bool wideBit = (opcode & 0b01);

                    Inst_Operand instOperand = Inst_ParseOperand((u8)fgetc(file));

                    // NOTE: Other reg values are not used.
                    Assert(instOperand.reg == 0b000);

                    Disassembly_Operand operandDest {0};
                    LoadMemoryOperand(file, &operandDest, instOperand.mod, instOperand.rm);

                    Disassembly_Operand operandSrc {0};
                    operandSrc.outputWidth = true;
                    LoadImmediateOperand(file, &operandSrc, wideBit, false);

                    PrintOperands(operandDest, operandSrc, wideBit);
                }
                else if ((opcode & 0b11111100) == MOV_RM)
                {
                    printf("mov ");
                    
                    bool directionBit = ((opcode >> 1) & 0b1);
                    bool wideBit = (opcode & 0b1);

                    Inst_Operand instOperand = Inst_ParseOperand(fgetc(file));

                    Disassembly_Operand operand1{0};
                    operand1.InitRegister(instOperand.reg);

                    Disassembly_Operand operand2{0};
                    LoadMemoryOperand(file, &operand2, instOperand.mod, instOperand.rm);

                    if (directionBit)
                    {
                        PrintOperands(operand1, operand2, wideBit);
                    }
                    else
                    {
                        PrintOperands(operand2, operand1, wideBit);
                    }
                }
                else if ((opcode & 0b11111100) == MOV_AX_MEMORY)
                // ANCHOR: MOV accumulator-memory
                // - MOV, Memory to accumulator
                // - MOV, Accumulator to memory
                {
                    // NOTE: The direction bit here has opposite meaning to the standard one, and it's written as 2 separate commands in the manual.
                    bool direction = ((opcode & 0b10) >> 1);
                    bool wide = (opcode & 0b01);

                    u16 AddressInMemory = Load16BitValue(file);

                    if (direction) 
                    {
                        printf("mov [%d], ax", AddressInMemory);
                    }
                    else
                    {
                        printf("mov ax, [%d]", AddressInMemory);
                    }
                }
                else if (opcode == 0b11000010) // RET - within seg adding immediate to SP
                {
                    i16 offset = (i16)Load16BitValue(file);
                    printf("ret %d", offset);
                }
                else if ((opcode & 0b11110000) == MOV_IMM_REG)
                {
                    printf("mov ");

                    bool wideBit = ((opcode & 0b00001000) >> 3);
                    RMField reg = (RMField)(opcode & 0b111);

                    Disassembly_Operand opDest {0};
                    opDest.InitRegister(reg);
                    Disassembly_Operand opSrc {0};
                    LoadImmediateOperand(file, &opSrc, wideBit, false);

                    PrintOperands(opDest, opSrc, wideBit);
                }
                else if ((opcode & 0b11110000) == 0b01110000)
                {
                    // TODO: Jump labels

                    char* jumpOpNames[] = {"jo", "jno", "jb", "jnb", "je", "jne", "jbe", "ja", "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg"};
                    u8 jumpType = (opcode & 0b1111);
                    i8 displacement = (i8)fgetc(file) + 2;

                    printf(jumpOpNames[jumpType]);

                    if (displacement >= 0)
                    {
                        printf(" $+%d", displacement);
                    }
                    else
                    {
                        printf(" $%d", displacement);
                    }
                }
                else if ((opcode & 0b11111100) == 0b11100000)
                {
                    u8 loopType = (opcode & 0b1111);
                    i8 displacement = (i8)fgetc(file) + 2;

                    if      (loopType == 0b00) printf("loopnz");
                    else if (loopType == 0b01) printf("loopz");
                    else if (loopType == 0b10) printf("loop");
                    else if (loopType == 0b11) printf("jcxz");
                    else
                    {
                        printf("UNKNOWN_OP_LOOPS");
                    }

                    if (displacement >= 0)
                    {
                        printf(" $+%d", displacement);
                    }
                    else
                    {
                        printf(" $%d", displacement);
                    }
                }
                else if ((opcode & 0b11111111) == 0b10001111) // pop
                {
                    Inst_Operand instOperand = Inst_ParseOperand(fgetc(file));
                    Assert(instOperand.reg == 0b000); // NOTE: Other values are not used.
                    
                    Disassembly_Operand operand {0};
                    LoadMemoryOperand(file, &operand, instOperand.mod, instOperand.rm);

                    printf("pop ");
                    if (operand.modField != REGISTER_MODE)
                    {
                        printf("word ");
                    }

                    PrintOperand(operand, true);
                }
                else if ((opcode & 0b11111110) == 0b11111110)
                {
                    char* opNames[] = {"inc", "dec", "call", "call", "jmp", "jmp", "push", "; invalid op"};

                    Inst_Operand instructionOperand = Inst_ParseOperand(Load8BitValue(file));
                    printf("%s ", opNames[instructionOperand.reg]);

                    bool wide = (opcode & 0b1);

                    Disassembly_Operand operand {0};
                    LoadMemoryOperand(file, &operand, instructionOperand.mod, instructionOperand.rm);

                    // NOTE: Push is always wide.
                    // STUDY: Is this explicit "word" really necessary?
                    if (instructionOperand.reg == 0b110)
                    {
                        printf("word ");
                    }
                    else if (instructionOperand.reg == 0b000 || instructionOperand.reg == 0b001) // inc/dec
                    {
                        operand.outputWidth = true;
                    }
                    PrintOperand(operand, wide);
                }
                else if ((opcode & 0b11111100) == 0b10011100)
                {
                    char* opNames[] = {"pushf", "popf", "sahf", "lahf"};
                    printf(opNames[(opcode & 0b11)]);
                }
                else
                {
                    printf("; %x", opcode);
                }
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