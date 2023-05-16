#include "common.cpp"

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

struct OperandByte
{
    union
    {
        u8 value;
        struct
        {
            ModField mod; // Operation mode. May correspond to the number of displacement bytes.
            RMField reg;  // Register or instruction index
            RMField rm;   // Register index or memory adressing type
        };
    };
};

enum OperandType
{
    OP_IMMEDIATE,
    OP_MEMORY,
    OP_REGISTER,
    OP_SEGMENT_REGISTER
};

struct Operand
{
    OperandType type;

    // TODO: Replace this with a pointer that's better for indexing into the CPU struct.
    RMField regmemIndex;
    ModField modField;

    // TODO: Remove. This flag can be determined from the instruction details.
    bool outputWidth;

    // TODO: Change sign. Unsigned may be better as a default.
    union
    {
        u16 value;
        struct
        {
            u8 valueLow; // NOTE: 8-bit operand is stored in low byte
            u8 valueHigh;
        };
    };
};

enum InstructionType
{
    DIS_NOOP,

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

};

static const char* operationNames[] = {
    "; NOOP", "mov", "push", "pop", "xchg", "in", "out", "xlat", "lea", "lds", "les", "lahf", "sahf", "pushf",
    "popf", "add", "adc", "inc", "aaa", "daa", "sub", "sbb", "dec", "neg", "cmp", "aas", "das", "mul", "imul",
    "aam", "div", "idiv", "aad", "cbw", "cwd", "not", "shl", "shr", "sar", "rol", "ror", "rcl", "rcr", "and",
    "test", "or", "xor", "rep", "movsb", "movsw", "cmpsb", "cmpsw", "scasb", "scasw", "lodsb", "lodsw", "stosb",
    "stosw", "call", "jmp", "ret", "je", "jl", "jle", "jb", "jbe", "jp", "jo", "js", "jne", "jnl", "jnle", "jnb",
    "jnbe", "jnp", "jno", "jns", "loop", "loopz", "loopnz", "jcxz", "int", "into", "iret", "clc", "cmc", "stc",
    "cld", "std", "cli", "sti", "hlt", "wait", "esc", "lock", "segment"
};

struct Instruction
{
    InstructionType type;
    int operandCount;

    Operand opDest;
    Operand opSrc;

    bool isWide;
};

OperandByte Inst_ParseOperand(u8 byte)
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

Operand InitRegisterOperand(RMField registerIndex)
{
    Operand result {0};
    result.type = OP_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Operand InitSegmentRegisterOperand(RMField registerIndex)
{
    Operand result {0};
    result.type = OP_SEGMENT_REGISTER;
    result.regmemIndex = registerIndex;
    return result;
}

Operand InitImmediateOperand(i16 value)
{
    Operand result {0};
    result.type = OP_IMMEDIATE;
    result.value = value;
    return result;
}
