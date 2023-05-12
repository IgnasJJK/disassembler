#include "common.cpp"

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

enum Disassembly_OperandType
{
    OP_IMMEDIATE,
    OP_MEMORY,
    OP_REGISTER,
    OP_SEGMENT_REGISTER
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
};

enum Disassembly_InstructionType
{
    // Data transfer
    DIS_MOV,
    DIS_PUSH,
    DIS_POP,
    DIS_XCHG,
    DIS_IN,
    DIS_OUT,
    DIS_XLAT,
    DIS_LEA,
    DIS_LDS,
    DIS_LES,
    DIS_LAHF,
    DIS_SAHF,
    DIS_PUSHF,
    DIS_POPF,
    // Arithmetic
    DIS_ADD,
    DIS_ADC,
    DIS_INC,
    DIS_AAA,
    DIS_DAA,
    DIS_SUB,
    DIS_SBB,
    DIS_DEC,
    DIS_NEG,
    DIS_CMP,
    DIS_AAS,
    DIS_DAS,
    DIS_MUL,
    DIS_IMUL,
    DIS_AAM,
    DIS_DIV,
    DIS_IDIV,
    DIS_AAD,
    DIS_CBW,
    DIS_CWD,
    // Logic
    DIS_NOT,
    DIS_SHL,
    DIS_SHR,
    DIS_SAR,
    DIS_ROL,
    DIS_ROR,
    DIS_RCL,
    DIS_RCR,
    DIS_AND,
    DIS_TEST,
    DIS_OR,
    DIS_XOR,
    // String manipulation
    DIS_REP,
    DIS_MOVS,
    DIS_CMPS,
    DIS_SCAS,
    DIS_LODS,
    DIS_STDS,
    // Control transfer
    DIS_CALL,
    DIS_JMP,
    DIS_RET,
    DIS_JE,
    DIS_JL,
    DIS_JLE,
    DIS_JB,
    DIS_JBE,
    DIS_JP,
    DIS_JO,
    DIS_JS,
    DIS_JNE,
    DIS_JNL,
    DIS_JNLE,
    DIS_JNB,
    DIS_JNBE,
    DIS_JNP,
    DIS_JNO,
    DIS_JNS,
    DIS_LOOP,
    DIS_LOOPZ,
    DIS_LOOPNZ,
    DIS_JCXZ,
    DIS_INT,
    DIS_INTO,
    DIS_IRET,
    // Processor control
    DIS_CLC,
    DIS_CMC,
    DIS_STC,
    DIS_CLD,
    DIS_STD,
    DIS_CLI,
    DIS_STI,
    DIS_HLT,
    DIS_WAIT,
    DIS_ESC,
    DIS_LOCK,
    DIS_SEGMENT,
};

static const char* operationNames[] = {
    "mov", "push", "pop", "xchg", "in", "out", "xlat", "lea", "lds", "les", "lahf", "sahf", "pushf", "popf",
    "add", "adc", "inc", "aaa", "daa", "sub", "sbb", "dec", "neg", "cmp", "aas", "das", "mul", "imul", "aam",
    "div", "idiv", "aad", "cbw", "cwd", "not", "shl", "shr", "sar", "rol", "ror", "rcl", "rcr", "and", "test",
    "or", "xor", "rep", "movs", "cmps", "scas", "lods", "stds", "call", "jmp", "ret", "je", "jl", "jle", "jb",
    "jbe", "jp", "jo", "js", "jne", "jnl", "jnle", "jnb", "jnbe", "jnp", "jno", "jns", "loop", "loopz", "loopnz",
    "jcxz", "int", "into", "iret", "clc", "cmc", "stc", "cld", "std", "cli", "sti", "hlt", "wait", "esc", "lock",
    "segment"
};

struct Disassembly_Instruction
{
    Disassembly_InstructionType type;
    int operandCount;

    Disassembly_Operand operand1;
    Disassembly_Operand operand2;

    bool isWide;
    bool switchOperands;
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
