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

#define PrintOperands(direction, op1str, op2str, op1, op2) \
if(direction) {\
    printf(op1str ", " op2str, op1, op2);\
} else {\
    printf(op2str ", " op1str, op2, op1);\
}

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

    BX_SI, // | AL | AX | BX + SI + (disp)
    BX_DI, // | CL | CX | BX + DI + (disp)
    BP_SI, // | DL | DX | BP + SI + (disp)
    BP_DI, // | BL | BX | BP + DI + (disp)
    SI,    // | AH | SP | SI + (disp)
    DI,    // | CH | BP | DI + (disp)
    BP,    // | DH | SI | BP + (disp) // NOTE: (If MOD == 110, then DISP is direct address)
    BX,    // | BH | DI | BX + (disp)
};
#define RM_DIRECT_ADDRESS 0b110

struct OperandByte
{
    ModField mod; // Operation mode. May correspond to the number of displacement bytes.
    RMField reg;
    RMField rm;
};

OperandByte ParseOperandByte(u8 byte)
{
    // Byte structure:
    // - mod: (2 bit) mode 
    // - reg: (3 bit) register
    // - r/m: (3 bit) register/memory

    OperandByte result;
    result.mod = (ModField)((byte >> 6) & 0b11);
    result.reg = (RMField)((byte >> 3) & 0b111);
    result.rm = (RMField)(byte & 0b111);

    return result;
}

inline u16 LoadValue16(FILE* file)
{
    u16 value = 0;
    *((u8 *)(&value) + 0) = (u8)fgetc(file);
    *((u8 *)(&value) + 1) = (u8)fgetc(file);
    return value;
}

i16 ParseDisplacement(FILE* file, u8 byteCount)
{
    Assert(byteCount == 0b10 || byteCount == 0b01);

    i16 Result = 0;

    if (byteCount == 1)
    {
        Result = (i16)fgetc(file);
    }
    else
    {
        Result = (i16)LoadValue16(file);
    }

    return Result;
}

void PrintAddressOperand(char* effectiveAddress, i8 displacement)
{
    printf("[%s %s %d]", effectiveAddress, 
        (displacement >= 0 ? "+" : "-"), 
        (displacement < 0 ? -displacement : displacement));
}

void PrintAddressOperand(char* effectiveAddress, i16 displacement)
{
    printf("[%s %s %d]", effectiveAddress, 
        (displacement >= 0 ? "+" : "-"), 
        (displacement < 0 ? -displacement : displacement));
}

int main(int argc, char** argv)
{
    char* registers8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    char* registers16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    char* effectiveAddress[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

    char* subOpType[] = {"add", "or", "adc", "sbb", "and", "sub", "UNKNOWN-OP", "cmp"};

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
                u8 byte1 = (u8)input;
                if ((byte1 & 0b11000100) == REG_RM_OP)
                {
                    bool directionBit = ((byte1 & 0b10) >> 1);
                    bool wideBit = (byte1 & 0b1);

                    OperandByte operand = ParseOperandByte(fgetc(file));

                    // Print operation type
                    u8 operation = (byte1 & OP_MASK) >> 3;
                    printf("%s ", subOpType[operation]);

                    char** regNames = (char**)(wideBit ? registers16 : registers8);

                    if (operand.mod == REGISTER_MODE)
                    {
                        PrintOperands(directionBit, "%s", "%s", regNames[operand.reg], regNames[operand.rm]);
                    }
                    else if (operand.mod == MEMORY_0BIT_MODE)
                    {
                        if (operand.rm == RM_DIRECT_ADDRESS)
                        {
                            i16 displacement = (i16)LoadValue16(file);
                            PrintOperands(directionBit, "%s", "[%d]", regNames[operand.reg], displacement);
                        }
                        else
                        {
                            PrintOperands(directionBit, "%s", "[%s]", regNames[operand.reg], effectiveAddress[operand.rm]);
                        }
                    }
                    else
                    {
                        i16 displacement = ParseDisplacement(file, operand.mod);
                        if (directionBit)
                        {
                            printf("%s, ", regNames[operand.reg]);
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                        }
                        else
                        {
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                            printf(", %s", regNames[operand.reg]);
                        }
                    }
                }
                else if ((byte1 & 0b11000100) == 0b00000100) // Immediate to accumulator
                {
                    u8 operation = ((byte1 & OP_MASK) >> 3);
                    printf("%s ", subOpType[operation]);

                    bool wideBit = (byte1 & 0b1);
                    if (wideBit)
                    {
                        i16 value = (i16)LoadValue16(file);
                        printf("ax, %d", value);
                    }
                    else
                    {
                        i8 value = (i8)fgetc(file);
                        printf("al, %d", value);
                    }
                }
                else if ((byte1 & 0b11111100) == 0b10000000) // Immediate to register/memory
                {
                    // ADD Immediate to register/memory: 100000sw [mod 000 r/m] [DISP-LO] [DISP-HI] [data] [data if s:w = 01]
                    // CMP Immediate with register/memory: 100000sw [mod 111 r/m] [DISP-LO] [DISP-HI] [data] [data if s:w = 01]
                    
                    bool signBit = ((byte1 >> 1) & 0b1);
                    bool wideBit = (byte1 & 0b1);

                    u8 modByte = (u8)fgetc(file);
                    OperandByte operand = ParseOperandByte(modByte);

                    char** regNames = (char**)(wideBit ? registers16 : registers8);

                    // NOTE: For immediate instructions, the REG part of the operand byte determines the operation type.
                    printf("%s ", subOpType[operand.reg]);
                    
                    if (operand.mod == REGISTER_MODE)
                    {
                        if (signBit)
                        {
                            i8 value = (i8)fgetc(file);
                            printf("%s, %d", regNames[operand.rm], value);
                        }
                        else
                        {
                            if (wideBit)
                            {
                                u16 value = LoadValue16(file);
                                printf("%s, %d", regNames[operand.rm], value);
                            }
                            else
                            {
                                u8 value = (u8)fgetc(file);
                                printf("%s, %d", regNames[operand.rm], value);
                            }
                        }
                    }
                    else if (operand.mod == MEMORY_0BIT_MODE)
                    {
                        printf((wideBit ? "word " : "byte "));

                        if (operand.rm == RM_DIRECT_ADDRESS)
                        {
                            i16 displacement = (i16)LoadValue16(file);
                            printf("[%d], ", displacement);
                        }
                        else
                        {
                            printf("[%s], ", effectiveAddress[operand.rm]);
                        }

                        if (wideBit && !signBit)
                        {
                            u16 value = LoadValue16(file);
                            printf("%d", value);
                        }
                        else
                        {
                            u8 value = (u8)fgetc(file);
                            printf("%d", value);
                        }
                    }
                    else
                    {
                        // immediate to memory with displacement
                        i16 displacement = ParseDisplacement(file, operand.mod);
                        if (wideBit)
                        {
                            printf("word ");
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);

                            i16 value = 0;
                            if (signBit)
                            {
                                i8* valuePtr = (i8*)(&value);
                                *(valuePtr) = (i8)fgetc(file);

                                if ((value >> 7) == 0b1)
                                {
                                    *(valuePtr + 1) = 0b11111111;
                                }
                            }
                            else value = LoadValue16(file);
                            printf(", %d", value);
                        }
                        else
                        {
                            printf("byte ");
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                            i8 value = (i8)fgetc(file);
                            printf(", %d",  value);
                        }
                    }
                }
                else if ((byte1 & 0b11111110) == MOV_IMM_MEM)
                {
                    printf("mov ");
                    bool wide = (byte1 & 0b01);

                    u8 byte2 = (u8) fgetc(file);
                    u8 mod = ((byte2 >> 6) & 0b11);
                    u8 regm = (byte2 & 0b111);

                    // NOTE: mod represents the number of displacement bytes. Since this is immediate to 
                    // memory, mod=0b11 does not occur.
                    if (mod == MEMORY_0BIT_MODE)
                    {
                        if (regm == 0b110)
                        {
                            u16 displacement = LoadValue16(file);
                            printf("[%d], ", displacement);
                        }
                        else
                        {
                            printf("[%s], ", effectiveAddress[regm]);
                        }
                    }
                    else
                    {
                        i16 displacement = ParseDisplacement(file, mod);
                        PrintAddressOperand(effectiveAddress[regm], displacement);
                        printf(", ");
                    }

                    // Print the value
                    if (wide)
                    {
                        u16 value = LoadValue16(file);
                        printf("word %d", value);
                    }
                    else
                    {
                        u8 value = (u8)fgetc(file);
                        printf("byte %d", value);
                    }
                }
                else if ((byte1 & 0b11111100) == MOV_RM)
                {
                    printf("mov ");
                    
                    // 1 = REG is the destination
                    // 0 = REG is the source
                    bool direction = ((byte1 & 0b10) >> 1);
                    bool wide = (byte1 & 0b01);

                    u8 byte2 = (u8) fgetc(file);

                    OperandByte operand = ParseOperandByte(byte2);

                    char** regNames = (char**)(wide ? registers16 : registers8);

                    // TODO: Reorganize around operand order

                    if (operand.mod == REGISTER_MODE) // Register-register
                    {
                        PrintOperands(direction, "%s", "%s", regNames[operand.reg], regNames[operand.rm]);
                    }
                    else if (operand.mod == MEMORY_0BIT_MODE)
                    {
                        if (operand.rm == 0b110)
                        {
                            i16 displacement = (i16)LoadValue16(file);
                            PrintOperands(direction, "%s", "[%d]", regNames[operand.reg], displacement);
                        }
                        else
                        {
                            PrintOperands(direction, "%s", "[%s]", regNames[operand.reg], effectiveAddress[operand.rm]);
                        }
                    }
                    else if (operand.mod == MEMORY_8BIT_MODE)
                    {
                        i8 displacement = ParseDisplacement(file, operand.mod);
                        if (direction)
                        {
                            printf("%s, ", regNames[operand.reg]);
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                        }
                        else
                        {
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                            printf(", %s", regNames[operand.reg]);
                        }
                    }
                    else // MEMORY_16BIT_MODE
                    {

                        i16 displacement = ParseDisplacement(file, operand.mod);
                        if (direction)
                        {
                            printf("%s, ", regNames[operand.reg]);
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                        }
                        else
                        {
                            PrintAddressOperand(effectiveAddress[operand.rm], displacement);
                            printf(", %s", regNames[operand.reg]);
                        }
                    }
                }
                else if ((byte1 & 0b11111100) == MOV_AX_MEMORY)
                // ANCHOR: MOV accumulator-memory
                // - MOV, Memory to accumulator
                // - MOV, Accumulator to memory
                {
                    // NOTE: The direction bit here has opposite meaning to the standard one, and it's written as 2 separate commands in the manual.
                    bool direction = ((byte1 & 0b10) >> 1);
                    bool wide = (byte1 & 0b01);

                    u16 AddressInMemory = LoadValue16(file);

                    if (direction) 
                    {
                        printf("mov [%d], ax", AddressInMemory);
                    }
                    else
                    {
                        printf("mov ax, [%d]", AddressInMemory);
                    }
                }
                else if ((byte1 & 0b11110000) == MOV_IMM_REG)
                {
                    printf("mov ");

                    bool wide = ((byte1 & 0b00001000) >> 3);
                    u8 reg = (byte1 & 0b111);

                    if (wide)
                    {
                        i16 value = (i16)LoadValue16(file);
                        printf("%s, %d", registers16[reg], value);
                    }
                    else
                    {
                        i8 value = (i8)fgetc(file);
                        printf("%s, %d", registers8[reg], value);
                    }
                }
                else if ((byte1 & 0b11110000) == 0b01110000)
                {
                    // TODO: Jump labels

                    char* jumpOpNames[] = {"jo", "jno", "jb", "jnb", "je", "jne", "jbe", "ja", "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg"};
                    u8 jumpType = (byte1 & 0b1111);
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
                else if ((byte1 & 0b11111100) == 0b11100000)
                {
                    u8 loopType = (byte1 & 0b1111);
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
                else
                {
                    printf("%x", byte1);
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