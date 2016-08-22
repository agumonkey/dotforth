#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define s8  int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t

#define Kilobytes(n) (n * 1024)

struct forth_word_definition
{
    char* WordStart;
    u32   WordLength;
    u32   InstructionID;
};

struct forth_instruction
{
    u32 Op;
    u32 Value;
};

#define IsNumeric(cc) (cc >= '0' && cc <= '9')
#define IsWhitespace(cc) (cc == ' ' || cc == '\n' || cc == '\t' || cc == '\r')

s32 
ConvertBase10StrInt(char* String, u32 Length)
{
    bool Negated = String[0] == '-';
    int Start = Negated ? 1 : 0;
    int Value = 0;
    int Multiplier = 1;
    for (int ii = Length-1; ii>=Start; --ii)
    {
        char cc = String[ii];
        Value += (cc - '0') * Multiplier;
        Multiplier *= 10;
    }

    if (Negated) Value = -Value;
    return Value;
}

u32 
ConvertStrHex(char* String, u32 Length)
{
    static const u32 HexConversionTable[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ,14, 15 };
    u32 Value = 0;
    for (int ii=0; ii<Length; ++ii)
    {
        char cc = String[ii];
        if (cc >= '0' && cc <= '9')
            cc -= '0';
        else if (cc >= 'a' && cc <= 'f')
            cc = cc - 'a' + 0xa;
        else if (cc >= 'A' && cc <= 'F')
            cc = cc - 'A' + 0xa;
        else
            printf("Str->Hex conversion error: %.*s\n", Length, String);
        Value |= HexConversionTable[(int)cc] << ((Length-ii-1)*4);
    }

    return Value;
}

s32 _strlen(char* Str)
{
    u32 Len = 0;
    for (int ii=0; Str[ii] != 0; ++ii)
    {
        Len++;
    } 
    return Len;
}

void 
PrintBytecodeStream(struct forth_instruction* Instructions, u32 Count, u32 BaseOffset)
{
    static char* OpNames[] = { 
        "PUSH",  "CALLW", "IADD", "ISUB", "IMUL", "IDIV", "DUP", "SWAP", "DROP",
        "OVER",  "DO",    "LOOP", "LCTR", "LOAD", "STORE", "RET",  "EOF" ,"RANGE",
        "FOR" ,  "PICK", 
    };
    
    for (int ii=0; ii<Count; ++ii)
    {
        struct forth_instruction Inst = Instructions[ii];    
        printf("%2d: OP_%s %.*s :: %d\n", BaseOffset+ii, OpNames[Inst.Op], (int)(10 - _strlen(OpNames[Inst.Op])), "                      ", Inst.Value);
    }
}

u32
CompileForth(char* String, int StringLength, char* BytecodeOut)
{
    int WordStart = 0;
    int WordEnd   = 0;
    struct forth_word_definition* WordDefinitions      = malloc(Kilobytes(32));
    struct forth_instruction*     WordInstructions     = malloc(Kilobytes(32));
    struct forth_instruction*     ProgramInstructions  = malloc(Kilobytes(32));
    struct forth_instruction*     InstructionStream    = ProgramInstructions;
    int WordDefinitionsCount     = 0;
    int WordInstructionsCount    = 0;
    int ProgramInstructionsCount = 0;
    int* InstructionStreamCount  = &ProgramInstructionsCount;
    struct forth_word_definition  CurrentWord = {0};

    bool CommentMode = false;
    for (int ii=0; ii<StringLength; ++ii)
    {
#define SkipWhitespace       while (ii < StringLength-1 &&  IsWhitespace(String[ii])) { ++ii;  }  
#define SkipUntilWhitespace  while (ii < StringLength-1 && !IsWhitespace(String[ii])) { ++ii; } 

#define OpWord(str) OpStr = str; if (_strlen(str) == WordLength && strncmp(WordStr, str, WordLength) == 0)
#define EmitOp(id, val) { InstructionStream[(*InstructionStreamCount)++] = (struct forth_instruction) {id, val}; }
        SkipWhitespace;
        WordStart = ii;
        SkipUntilWhitespace;
        WordEnd   = ii;
        char* OpStr = 0; 

        if (WordStart == WordEnd)
            break;

        int   WordLength = WordEnd-WordStart;
        char* WordStr    = String+WordStart;

        OpWord("[[")       { CommentMode = true;  continue; } 
        OpWord("]]")       { CommentMode = false; continue; } 
        if (CommentMode)   { continue; }

        OpWord("+")       { EmitOp(2, 0);  continue; } 
        OpWord("-")       { EmitOp(3, 0);  continue; } 
        OpWord("*")       { EmitOp(4, 0);  continue; } 
        OpWord("/")       { EmitOp(5, 0);  continue; } 
        OpWord("dup")     { EmitOp(6, 0);  continue; } 
        OpWord("swap")    { EmitOp(7, 0);  continue; } 
        OpWord("drop")    { EmitOp(8, 0);  continue; } 
        OpWord("over")    { EmitOp(9, 0);  continue; } 
        OpWord("do")      { EmitOp(10, 0); continue; } 
        OpWord("loop")    { EmitOp(11, 0); continue; }
        OpWord("lctr")    { EmitOp(12, 0); continue; }
        OpWord("@l")      { EmitOp(13, 0); continue; } // load
        OpWord("@s")      { EmitOp(14, 0); continue; } // store
        OpWord("range")   { EmitOp(17, 0); continue; } 
        OpWord("for")     { EmitOp(18, 0); continue; } 
        OpWord("pick")    { EmitOp(19, 0); continue; }
        OpWord(":")                       
        {
            InstructionStream      = WordInstructions;
            InstructionStreamCount = &WordInstructionsCount;
            int WordOffset = ++ii;
            SkipWhitespace;
            CurrentWord.InstructionID = WordInstructionsCount;
            CurrentWord.WordStart     = String+ii;
            SkipUntilWhitespace;
            CurrentWord.WordLength    = ii - WordOffset;
            continue;
        }
        OpWord(";")                       // 13
        {
            EmitOp(15, 0); 
            //CurrentWord.InstructionCount  = WordInstructionsCount - CurrentWord.InstructionID;
            //printf("WORD DEF: %.*s :: %d instructions\n", CurrentWord.WordLength, CurrentWord.WordStart, CurrentWord.InstructionCount);
            WordDefinitions[WordDefinitionsCount++] = CurrentWord;
            InstructionStream         = ProgramInstructions;
            InstructionStreamCount    = &ProgramInstructionsCount;
            continue;
        }

        if (WordStr[0] == ',')
        {
            //printf("HEX: %.*s\n", WordLength, WordStr);
            EmitOp(0, ConvertStrHex(WordStr+1, WordLength-1));
            continue;
        }
        else
        {
            bool WordIsInteger = true;
            for (int jj=0; jj<WordLength; ++jj)
            {
                if (!IsNumeric(WordStr[jj]) && WordStr[jj] != '-')
                {
                    WordIsInteger = false;
                    break;
                }
            }
            if (WordIsInteger)
            {
                //printf("INTEGER: %.*s\n", WordLength, WordStr);
                EmitOp(0, ConvertBase10StrInt(WordStr, WordLength));
            }
            else
            {
                //printf("WORD: %.*s\n", WordLength, WordStr);
                for (int ii=0; ii<WordDefinitionsCount; ++ii)
                {
                    struct forth_word_definition Def = WordDefinitions[ii];
                    //printf("%d :: %d\n", WordLength, Def.WordLength);
                    if (Def.WordLength == WordLength && strncmp(Def.WordStart, WordStr, WordLength) == 0)
                    {
                        //printf("EMIT: %.*s\n", WordLength, WordStr);
                        // Word is defined, compile jump instruction
                        EmitOp(1, Def.InstructionID);
                        break;
                    }
                }
            }
        }
    }

    InstructionStream         = ProgramInstructions;
    InstructionStreamCount    = &ProgramInstructionsCount;
    EmitOp(16, 0);  // OP_EOF

    // Fix the offsets for the program + word codes
    for (int ii=0; ii<ProgramInstructionsCount; ++ii)
    {
        struct forth_instruction* Instr = &ProgramInstructions[ii];
        if (Instr->Op == 1) // op is callw
        {
            Instr->Value += ProgramInstructionsCount;
        }
    }

    for (int ii=0; ii<WordInstructionsCount; ++ii)
    {
        struct forth_instruction* Instr = &WordInstructions[ii];
        if (Instr->Op == 1) // op is callw
        {
            Instr->Value += ProgramInstructionsCount;
        }
    }

    printf("Program instructions\n");
    PrintBytecodeStream(ProgramInstructions, ProgramInstructionsCount, 0);
    printf("Word instructions\n");
    PrintBytecodeStream(WordInstructions,    WordInstructionsCount, ProgramInstructionsCount);

    u32 Offset = 0;
    u32 Size   = sizeof(struct forth_instruction) * ProgramInstructionsCount;
    memcpy(BytecodeOut, ProgramInstructions, Size);         Offset += Size;
    Size       = sizeof(struct forth_instruction) * WordInstructionsCount;
    memcpy(BytecodeOut+Offset, WordInstructions, Size);  Offset += Size;

    free(WordDefinitions);
    free(WordInstructions);
    free(ProgramInstructions);
    return Offset;
}

int main(int argc, char** argv)
{
    static char ForthSource[Kilobytes(32)];
    if (argc < 2) 
    { 
        fgets(ForthSource, sizeof ForthSource, stdin);
    }
    else
    {
        FILE* InputFile = fopen(argv[1], "r");
        fread(ForthSource, 1, sizeof ForthSource, InputFile);
        fclose(InputFile);
    }

    static char BytecodeStream[Kilobytes(64)];
    u32 BytecodeStreamSize = CompileForth(ForthSource, sizeof ForthSource, BytecodeStream);
    FILE* BytecodeFile = fopen("bytecode", "wb");
    fwrite(BytecodeStream, BytecodeStreamSize, 1, BytecodeFile);
    fclose(BytecodeFile);
    return 0;    
}

