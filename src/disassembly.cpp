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
#define REG_AL MEM_BX_SI
#define REG_BX MEM_BP_DI
#define REG_BL MEM_BP_DI
#define REG_CX MEM_BX_DI
#define REG_CL MEM_BX_DI
#define REG_DX MEM_BP_SI
#define REG_DL MEM_BP_SI

#define REG_SP MEM_SI
#define REG_BP MEM_DI
#define REG_SI MEM_BP
#define REG_DI MEM_BX
#define REG_AH MEM_SI
#define REG_CH MEM_DI
#define REG_DH MEM_BP
#define REG_BH MEM_BX

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

    // TODO: Remove. This flag can be determined from the instruction details.
    bool outputWidth;

    // TODO: Change sign. Unsigned may be better as a default.
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
    DIS_MOVSB,
    DIS_MOVSW,
    DIS_CMPSB,
    DIS_CMPSW,
    DIS_SCASB,
    DIS_SCASW,
    DIS_LODSB,
    DIS_LODSW,
    DIS_STOSB,
    DIS_STOSW,
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

    DIS_NOOP
};

static const char* operationNames[] = {
    "mov", "push", "pop", "xchg", "in", "out", "xlat", "lea", "lds", "les", "lahf", "sahf", "pushf", "popf",
    "add", "adc", "inc", "aaa", "daa", "sub", "sbb", "dec", "neg", "cmp", "aas", "das", "mul", "imul", "aam",
    "div", "idiv", "aad", "cbw", "cwd", "not", "shl", "shr", "sar", "rol", "ror", "rcl", "rcr", "and", "test",
    "or", "xor", "rep", "movsb", "movsw", "cmpsb", "cmpsw", "scasb", "scasw", "lodsb", "lodsw", "stosb", "stosw",
    "call", "jmp", "ret", "je", "jl", "jle", "jb", "jbe", "jp", "jo", "js", "jne", "jnl", "jnle", "jnb", "jnbe",
    "jnp", "jno", "jns", "loop", "loopz", "loopnz", "jcxz", "int", "into", "iret", "clc", "cmc", "stc", "cld", 
    "std", "cli", "sti", "hlt", "wait", "esc", "lock", "segment", ";noop"
};

struct Disassembly_Instruction
{
    Disassembly_InstructionType type;
    int operandCount;

    // TODO: Remove unions when switchOperands is removed.
    union
    {
        Disassembly_Operand operand1;
        Disassembly_Operand opDest;
    };
    union
    {
        Disassembly_Operand operand2;
        Disassembly_Operand opSrc;
    };

    bool isWide;

    // TODO: It's not necessary to store the switchOperands flag in the instruction, because
    // it only determines how the instruction would be written. The operation is sematically
    // oriented only SRC -> DEST. (This can be done when parsing)
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

Disassembly_Operand InitRegisterOperand(RMField registerIndex)
{
    Disassembly_Operand result {0};
    result.type = OP_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Disassembly_Operand InitSegmentRegisterOperand(RMField registerIndex)
{
    Disassembly_Operand result {0};
    result.type = OP_SEGMENT_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Disassembly_Operand InitImmediateOperand(i16 value)
{
    Disassembly_Operand result {0};
    result.type = OP_IMMEDIATE;
    result.value = value;
    return result;
}
