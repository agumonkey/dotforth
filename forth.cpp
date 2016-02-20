#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "util.c"

enum
{
    TYPE_NONE,
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
static const char* VerbWords[] =
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

static u16 VerbLengths[ARRAY_SIZE(VerbNames)];

typedef struct 
{
    char*  ExecData; 
    u16*   ExecTypes; 
    u32    Data[32]; 
    u32    _Guard[32]; 
    u32    ExecCount;
    u32    Count;
    char*  StringsData; 
    u32    StringsOffset;
} forth_ctx;

void InitForth(forth_ctx* Forth)
{
    Forth->ExecData  = (char*)malloc(Megabytes(1));
    Forth->StringsData  = (char*)malloc(Megabytes(1));
    Forth->ExecTypes = (u16*)malloc(Megabytes(1));
}

void FreeForth(forth_ctx* Forth)
{
    free(Forth->ExecData);
    free(Forth->StringsData);
    free(Forth->ExecTypes);
}

#define min(a,b) a<b?a:b

void ParseForthWord(char* String, u32 Length, void* Data, u16 WordType)
{
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
    else if(WordType == TYPE_VERB)
    {
        u32 Verb = 0;
        for (int ii=1; ii<ARRAY_SIZE(VerbWords); ++ii)
        {
            const char* VerbWord = VerbWords[ii];
            u16 VerbLength = VerbLengths[ii];
            if (Length == VerbLength && strncmp(VerbWord, String, Length) == 0)
            {
                Verb = ii;
                break;
            }
        }
        (*(u32*)(Data)) = Verb;
        LOG("%s\n", VerbNames[Verb]);
    }
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
    bool EscapeNext = false;

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
            if (WordType == TYPE_NONE)
            {
                WordType = TYPE_VERB;
            }
            ParseForthWord(&ProgramString[WordBegin]
                    , Offset - WordBegin
                    , &Forth->ExecData[Forth->ExecCount * sizeof(u32)]
                    , WordType);
            Forth->ExecTypes[Forth->ExecCount] = WordType;
            Forth->ExecCount++;
            WordBegin = Offset+1;
            WordType = TYPE_NONE;
        }

        if (Char == '\\')
        {
            EscapeNext = true;
        }
        else if (EscapeNext)
        {
            EscapeNext = false;
        }

        Offset++;
    }
}

void ExecuteForth(forth_ctx* Forth)
{
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

    for (int ii=1; ii<ARRAY_SIZE(VerbWords); ++ii)
    {
        VerbLengths[ii] = strlen(VerbWords[ii]);
    }

    forth_ctx Forth = {0};
    InitForth(&Forth);
    LoadForth(&Forth, File.Contents, File.ContentsSize);
    FreeForth(&Forth);


    return 0;
}
