#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "util.c"

#include "xxhash.h"

enum
{
    TYPE_NONE,
    TYPE_BUILTIN_VERB,
    TYPE_VERB,
    TYPE_INTEGER,
    TYPE_FLOAT,
};

#define VERBS \
    XX(NONE,  ) \
    XX(IADD, +) \
    XX(ISUB, -) \
    XX(IMUL, *) \
    XX(IDIV, /) \
    XX(IMOD, mod) \
    XX(FADD, .+) \
    XX(FSUB, .-) \
    XX(FMUL, .*) \
    XX(FDIV, ./) \
               
#define XX(name, word) VERB_##name,
enum
{
    VERBS
};
#undef XX

#define XX(name, word) #word,
static const char* BuiltinVerbWords[] =
{
    VERBS
};
#undef XX

#define XX(name, word) "VERB_"#name,
static const char* VerbNames[] =
{
    VERBS
};
#undef XX

static u16 BuiltinVerbLengths[ARRAY_SIZE(VerbNames)];

struct forth_verb_entry
{
    u32 StartOffset;
    u32 EndOffset;
};

typedef struct 
{
    char*  ExecData; 
    u16*   ExecTypes; 
    u32    Data[32]; 
    u32    _Guard0[32]; 
    u16    DataTypes[32]; 
    u32    _Guard1[32]; 
    u32    ExecCount;
    u32    Count;
    u64*   DictionaryKeys;
    forth_verb_entry* VerbEntries;
} forth_ctx;

void InitForth(forth_ctx* Forth)
{
    Forth->ExecData  = (char*)malloc(Megabytes(1));
    Forth->ExecTypes = (u16*)malloc(Megabytes(1));
    Forth->DictionaryKeys = (u64*)malloc(Megabytes(1));
    Forth->VerbEntries = (forth_verb_entry*)malloc(Megabytes(1));
}

void FreeForth(forth_ctx* Forth)
{
    free(Forth->ExecData);
    free(Forth->ExecTypes);
    free(Forth->DictionaryKeys);
    free(Forth->VerbEntries);
}

#define min(a,b) a<b?a:b

s32 WordIsBuiltinVerb(char* String, u32 Length)
{
    s32 Verb = -1;
    for (int ii=1; ii<ARRAY_SIZE(BuiltinVerbWords); ++ii)
    {
        const char* VerbWord = BuiltinVerbWords[ii];
        u16 VerbLength = BuiltinVerbLengths[ii];
        if (Length == VerbLength && strncmp(VerbWord, String, Length) == 0)
        {
            Verb = ii;
            break;
        }
    }
    return Verb;
}

inline bool IsNumeric(char Char)
{
    return (Char >= '0' && Char <= '9');
}

inline bool IsWhitespace(char Char)
{
    return (Char == ' ' || Char == '\t' || Char == '\n');
}

void LoadForth(forth_ctx* Forth, char* ProgramString, size_t Length)
{
    u32 Offset = 0;
    u32 WordBegin = 0;
    u16 WordType = TYPE_NONE;

    while (Offset < Length)
    {
        char Char = ProgramString[Offset];
        if (WordBegin == Offset && IsWhitespace(Char))
        {
            while (Offset <= Length && IsWhitespace(ProgramString[Offset])) 
            {
                Offset++; 
            }

            if (Offset == Length)
            {
                break;
            }

            Char = ProgramString[Offset];
            WordBegin = Offset;
        }

        if (WordBegin == Offset && IsNumeric(Char))
        {
            WordType = TYPE_INTEGER; 
        }
        else if(WordBegin == Offset && Char == '-' && IsNumeric(ProgramString[Offset+1]))
        {
            WordType = TYPE_INTEGER;
        }
        else if(WordType == TYPE_INTEGER && Char == '.')
        {
            WordType = TYPE_FLOAT;
        }
        else if(IsWhitespace(Char) || Offset == Length)
        {
            char* String = &ProgramString[WordBegin];
            u32   WordLength = Offset - WordBegin;
            void* Data = &Forth->ExecData[Forth->ExecCount * sizeof(u32)];

            if (WordType == TYPE_INTEGER)
            {
                u32 Value = strtol(String, 0, 10);
                (*(u32*)(Data)) = Value;
                LOG("INT %d\n", Value);
            }
            else if(WordType == TYPE_FLOAT)
            {
                f32 Value = strtof(String, 0);
                (*(f32*)(Data)) = Value;
                LOG("FLOAT %f\n", Value);
            }
            else if (WordType == TYPE_NONE)
            {
                s32 Verb = WordIsBuiltinVerb(String, WordLength);
                if (Verb != -1)
                {
                    WordType = TYPE_BUILTIN_VERB;
                    (*(u32*)(Data)) = Verb;
                    LOG("%s\n", VerbNames[Verb]);
                }
                else
                {
                    WordType = TYPE_VERB;
                    LOG("USER VERB: %.*s\n", WordLength, String);
                }
            }

            Forth->ExecTypes[Forth->ExecCount] = WordType;
            Forth->ExecCount++;
            WordBegin = Offset+1;
            WordType = TYPE_NONE;
        }

        Offset++;
    }
}

#define Word0 Forth->Count-1
#define Word1 Forth->Count-2
#define Word2 Forth->Count-3
#define Word3 Forth->Count-4

void ExecuteForth(forth_ctx* Forth)
{
    int ExecCount = Forth->ExecCount;
    u32* Integers = (u32*)Forth->ExecData;
    f32* Floats   = (f32*)Forth->ExecData;
    u32* StackInt   = (u32*)Forth->Data;
    f32* StackFloat = (f32*)Forth->Data;
    u16* StackTypes = (u16*)Forth->DataTypes;
    for (int ii=0; ii<ExecCount; ++ii)
    {
        u16 WordType = Forth->ExecTypes[ii];
        if (WordType == TYPE_INTEGER)
        {
            StackTypes[Forth->Count] = TYPE_INTEGER;
            StackInt[Forth->Count++] = Integers[ii];
        }
        else if (WordType == TYPE_FLOAT)
        {
            StackTypes[Forth->Count] = TYPE_FLOAT;
            StackFloat[Forth->Count++] = Floats[ii];
        }
        else if (WordType == TYPE_BUILTIN_VERB)
        {
            u32 Verb = Integers[ii];
            if (Verb == VERB_IADD)
            {
                u32 Result = StackInt[Word1] + StackInt[Word0];
                StackInt[Word1] = Result;
                StackTypes[Word1] = TYPE_INTEGER;
                Forth->Count--;
            }
            else if (Verb == VERB_ISUB)
            {
                u32 Result = StackInt[Word1] - StackInt[Word0];
                StackInt[Word1] = Result;
                StackTypes[Word1] = TYPE_INTEGER;
                Forth->Count--;
            }
            else if (Verb == VERB_IMUL)
            {
                u32 Result = StackInt[Word1] * StackInt[Word0];
                StackInt[Word1] = Result;
                StackTypes[Word1] = TYPE_INTEGER;
                Forth->Count--;
            }
            else if (Verb == VERB_IDIV)
            {
                u32 Result = StackInt[Word1] / StackInt[Word0];
                StackInt[Word1] = Result;
                StackTypes[Word1] = TYPE_INTEGER;
                Forth->Count--;
            }
            else if (Verb == VERB_FADD)
            {
                f32 Result = StackFloat[Word1] + StackFloat[Word0];
                StackFloat[Word1] = Result;
                StackTypes[Word1] = TYPE_FLOAT;
                Forth->Count--;
            }
            else if (Verb == VERB_FSUB)
            {
                f32 Result = StackFloat[Word1] - StackFloat[Word0];
                StackFloat[Word1] = Result;
                StackTypes[Word1] = TYPE_FLOAT;
                Forth->Count--;
            }
            else if (Verb == VERB_FMUL)
            {
                f32 Result = StackFloat[Word1] * StackFloat[Word0];
                StackFloat[Word1] = Result;
                StackTypes[Word1] = TYPE_FLOAT;
                Forth->Count--;
            }
            else if (Verb == VERB_FDIV)
            {
                f32 Result = StackFloat[Word1] / StackFloat[Word0];
                StackFloat[Word1] = Result;
                StackTypes[Word1] = TYPE_FLOAT;
                Forth->Count--;
            }
        }
    }

    LOG("On stack after execution\n");
    for (int ii=0; ii<Forth->Count; ++ii)
    {
        u16 WordType = Forth->DataTypes[ii];
        if (WordType == TYPE_INTEGER)
        {
            LOG("INT %d\n", StackInt[ii]);
        }
        else if(WordType == TYPE_FLOAT)
        {
            LOG("FLOAT %f\n", StackFloat[ii]);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return -1;
    }
    static char FileMemory[Megabytes(1)];
    load_file_result File = LoadFileIntoMemory(argv[1], FileMemory, sizeof(FileMemory));

    if (File.Contents == 0)
    {
        return -1;
    }

    for (int ii=1; ii<ARRAY_SIZE(BuiltinVerbWords); ++ii)
    {
        BuiltinVerbLengths[ii] = strlen(BuiltinVerbWords[ii]);
    }

    forth_ctx Forth = {0};
    InitForth(&Forth);
    LoadForth(&Forth, File.Contents, File.ContentsSize);
    ExecuteForth(&Forth);
    FreeForth(&Forth);

    return 0;
}
