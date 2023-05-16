#include "common.cpp"

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

    // Flags (2-7)
    union
    {
        u16 flags;
        struct {
            bool carry : 1;
            bool parity : 1;
            bool auxCarry : 1;
            bool zero : 1;
            bool sign : 1;
            bool overflow : 1;
            bool interruptEnable : 1;
            bool direction : 1;
            bool trap : 1;
        };
    };

    void* GetPointerToRegister(Operand* op, bool wide)
    {
        Assert(op->type == OP_REGISTER || op->type == OP_SEGMENT_REGISTER);
        Assert((int)op->regmemIndex >= 0);

        if (op->type == OP_REGISTER)
        {
            Assert((int)op->regmemIndex < 8);

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
        return nullptr;
    }
};

void PrintFlags(CPU cpu)
{
    if (cpu.carry) printf("C");
    if (cpu.parity) printf("P");
    if (cpu.auxCarry) printf("A");
    if (cpu.zero) printf("Z");
    if (cpu.sign) printf("S");
    if (cpu.overflow) printf("O");
    if (cpu.interruptEnable) printf("I");
    if (cpu.direction) printf("D");
    if (cpu.trap) printf("T");
}