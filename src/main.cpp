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
#define MOV_AX_MEM 0b10100000

// ADD, CMP, SUB (immediate)
#define ARITHMETIC_IMM 0b10000000 // 100000sw

// Immediate to accumulator
#define ACCUM_IMM_OP 0b00000100 // NOTE: Includes OP_MASK (00 OPC 100)

// REG/MEM Operation (ADD, CMP, SUB)
#define REG_RM_OP 0b00000000 // NOTE: Includes OP_MASK (00 OPC 000)

inline u16 LoadValue16(FILE* file)
{
    u16 value = 0;
    *((u8 *)(&value) + 0) = (u8)fgetc(file);
    *((u8 *)(&value) + 1) = (u8)fgetc(file);
    return value;
}

i16 GetDisplacement(FILE* file, u8 byteCount)
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

    char* subOpType[] = {"add", "", "", "", "", "sub", "", "cmp"};

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

                    u8 modByte = (u8)fgetc(file);
                    u8 mod = ((modByte >> 6) & 0b11);
                    u8 reg = ((modByte >> 3) & 0b111);
                    u8 regm = (modByte & 0b111);

                    u8 opType = (byte1 & OP_MASK) >> 3;

                    printf("%s ", subOpType[opType]);

                    char** regNames = (char**)(wideBit ? registers16 : registers8);

                    if (mod == 0b11) // Register-register
                    {
                        if (directionBit)
                        {
                            printf("%s, %s", regNames[reg], regNames[regm]);
                        }
                        else
                        {
                            printf("%s, %s", regNames[regm], regNames[reg]);
                        }
                    }
                    else if (mod == 0b00)
                    {
                        if (regm == 0b110)
                        {
                            i16 displacement = (i16)LoadValue16(file);
                            if (directionBit)
                            {
                                printf("%s, [%d]", regNames[reg], displacement);
                            }
                            else
                            {
                                printf("[%d], %s", displacement, regNames[reg]);
                            }
                        }
                        else
                        {
                            if (directionBit)
                            {
                                printf("%s, [%s]", regNames[reg], effectiveAddress[regm]);
                            }
                            else
                            {
                                printf("[%s], %s", effectiveAddress[regm], regNames[reg]);
                            }
                        }
                    }
                    else
                    {
                        i16 displacement = GetDisplacement(file, mod);
                        if (directionBit)
                        {
                            printf("%s, ", regNames[reg]);
                            PrintAddressOperand(effectiveAddress[regm], displacement);
                        }
                        else
                        {
                            PrintAddressOperand(effectiveAddress[regm], displacement);
                            printf(", %s", regNames[reg]);
                        }
                    }
                }
                else if ((byte1 & 0b11111100) == ARITHMETIC_IMM)
                {
                    bool signBit = ((byte1 & 0b10) >> 1);
                    bool wideBit = (byte1 & 0b1);

                    u8 modByte = (u8)fgetc(file);
                    u8 mod = ((modByte >> 6) & 0b11);
                    u8 subOpcode = ((modByte >> 3) & 0b111);
                    u8 regm = (modByte & 0b111);

                    printf("%s TODO", subOpType[subOpcode]);

                    // TODO: Parse operands
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
                    if (mod == 0b00)
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
                        i16 displacement = GetDisplacement(file, mod);
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

                    u8 mod = ((byte2 >> 6) & 0b11);
                    u8 reg = ((byte2 >> 3) & 0b111);
                    u8 regm = (byte2 & 0b111);

                    char** regNames = (char**)(wide ? registers16 : registers8);

                    // TODO: Reorganize around operand order

                    if (mod == 0b11) // Register-register
                    {
                        if (direction)
                        {
                            printf("%s, %s", regNames[reg], regNames[regm]);
                        }
                        else
                        {
                            printf("%s, %s", regNames[regm], regNames[reg]);
                        }
                    }
                    else if (mod == 0b00)
                    {
                        if (regm == 0b110)
                        {
                            i16 displacement = (i16)LoadValue16(file);
                            if (direction)
                            {
                                printf("%s, [%d]", regNames[reg], displacement);
                            }
                            else
                            {
                                printf("[%d], %s", displacement, regNames[reg]);
                            }
                        }
                        else
                        {
                            if (direction)
                            {
                                printf("%s, [%s]", regNames[reg], effectiveAddress[regm]);
                            }
                            else
                            {
                                printf("[%s], %s", effectiveAddress[regm], regNames[reg]);
                            }
                        }
                    }
                    else
                    {
                        i16 displacement = GetDisplacement(file, mod);
                        if (direction)
                        {
                            printf("%s, ", regNames[reg]);
                            PrintAddressOperand(effectiveAddress[regm], displacement);
                        }
                        else
                        {
                            PrintAddressOperand(effectiveAddress[regm], displacement);
                            printf(", %s", regNames[reg]);
                        }
                    }
                }
                else if ((byte1 & 0b11111100) == MOV_AX_MEM)
                {
                    printf("mov ");
                    
                    bool direction = ((byte1 & 0b10) >> 1);
                    bool wide = (byte1 & 0b01);

                    u16 address = LoadValue16(file);
                    // NOTE: Not written as d-bit in the manual, because the meaning is inverted. (i.e. d:1 ==> memory is the target)
                    if (direction)
                    {
                        printf("[%d], ax", address);
                    }
                    else
                    {
                        printf("ax, [%d]", address);
                    }
                }
                else if ((byte1 & 0b11110000) == MOV_IMM_REG)
                {
                    printf("mov ");

                    bool wide = ((byte1 & 0b00001000) >> 3);
                    u8 reg = (byte1 & 0b111);

                    if (wide)
                    {
                        u16 value = LoadValue16(file);
                        printf("%s, %d", registers16[reg], value);
                    }
                    else
                    {
                        u8 value = (u8)fgetc(file);
                        printf("%s, %d", registers8[reg], value);
                    }
                }
                else
                {
                    printf("%x ", byte1);
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