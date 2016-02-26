#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "util.c"

#include "xxhash.h"

enum word_type
{
    TYPE_NONE,
    TYPE_CORE_VERB,
    TYPE_VERB,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_VERB_DEFINITION_ENTRY, // Which entry we are replacing with
    TYPE_VERB_DEFINITION_CODEPOINT, 
};

#define CORE_VERBS \
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
    XX(DEFINITION_START, ::) \
    XX(DEFINITION_END,   ;;) \

#define XX(name, word) VERB_##name,
enum
{
    CORE_VERBS
};
#undef XX

#define XX(name, word) #word,
static const char* CoreVerbWords[] =
{
    CORE_VERBS
};
#undef XX

#define XX(name, word) "VERB_"#name,
static const char* CoreVerbNames[] =
{
    CORE_VERBS
};
#undef XX

static u64 CoreVerbHashes[ARRAY_SIZE(CoreVerbNames)];

struct forth_verb_offset
{
    u32 StartOffset;
    u32 WordCount;
};

typedef struct 
{
    char*  ExecData;  // Storage for program words
    u16*   ExecTypes; 
    u32    ExecCount;

    char*  VerbExecData;  // Storage for program words
    u16*   VerbExecTypes; 
    u32    VerbExecCount;

    u64* VerbNameHashes;
    forth_verb_offset* VerbOffsetEntries;
    u32 VerbsCount;

    // Stack
    u32    Data[32]; 
    u32    _Guard0[32]; 
    u16    DataTypes[32]; 
    u32    _Guard1[32]; 
    u32    Count;   // # of items on the stack
} forth_ctx;

void InitForth(forth_ctx* Forth)
{
    Forth->ExecData  = (char*)malloc(Megabytes(1));
    Forth->ExecTypes = (u16*)malloc(Megabytes(1));
    Forth->VerbExecData  = (char*)malloc(Megabytes(1));
    Forth->VerbExecTypes = (u16*)malloc(Megabytes(1));
    Forth->VerbNameHashes = (u64*)malloc(Megabytes(1));
    Forth->VerbOffsetEntries = (forth_verb_offset*)malloc(Megabytes(1));
}

void FreeForth(forth_ctx* Forth)
{
    free(Forth->ExecData);
    free(Forth->ExecTypes);
    free(Forth->VerbExecData);
    free(Forth->VerbExecTypes);
    free(Forth->VerbNameHashes);
    free(Forth->VerbOffsetEntries);
}

#define min(a,b) a<b?a:b

u32 WordIsCoreVerb(u64 Hash)
{
    u32 Verb = 0;
    for (int ii=1; ii<ARRAY_SIZE(CoreVerbWords); ++ii)
    {
        u64 VerbHash = CoreVerbHashes[ii];
        if (Hash == VerbHash)
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

u32 FindVerbEntry(forth_ctx* Forth, char* VerbString, u32 VerbStringLength)
{
    u64 Hash = XXH64(VerbString, VerbStringLength, 0xdeadc0de);
    u32 VerbsCount = Forth->VerbsCount;
    u64* VerbNameHashes = Forth->VerbNameHashes;
    for (u32 ii=0; ii<VerbsCount; ++ii)
    {
        if (VerbNameHashes[ii] == Hash)
        {
            return ii;
        }
    }
    return 0xffffffff;
}

void LoadForth(forth_ctx* Forth, char* ProgramString, size_t ProgramLength)
{
    u32 Offset = 0;
    int VerbDefinitionStage = 0;
    u32 CurrentVerbEntry = 0;
    u32 CurrentVerbStartOffset = 0;

    static u32 WordStartOffsets[128*1024];
    static u32 WordLengths[128*1024];
    static u32 WordsCount;

    while (Offset < ProgramLength)
    {
        while (IsWhitespace(ProgramString[Offset]))
        {
            ++Offset;
        }
        u32 StartOffset = Offset;
        while (!IsWhitespace(ProgramString[Offset]) && Offset < ProgramLength)
        {
            ++Offset;
        }

        if (Offset < ProgramLength)
        {
            u32 EndOffset = Offset;
            WordStartOffsets[WordsCount] = StartOffset;
            WordLengths[WordsCount] = EndOffset - StartOffset;
            ++WordsCount;
        }
    }

    for (u32 ii=0; ii<WordsCount; ++ii)
    {
        char* WordString = ProgramString + WordStartOffsets[ii];
        u32 WordLength = WordLengths[ii];
        int WordType = TYPE_NONE;
        void* DefinitionData = NULL;

        for (u32 jj=0; jj<WordLength; ++jj)
        {
            char Char = WordString[jj];
            if (jj == 0)
            {
                if (IsNumeric(Char) || (Char == '-' && IsNumeric(WordString[jj+1])))
                {
                    WordType = TYPE_INTEGER;
                }
            }
            else if (WordType == TYPE_INTEGER && Char == '.')
            {
                WordType = TYPE_FLOAT;
                break;
            }
        }

        if (VerbDefinitionStage == 0)
        {
            DefinitionData = &Forth->ExecData[Forth->ExecCount * sizeof(u32)];
        }
        else
        {
            DefinitionData = &Forth->VerbExecData[Forth->VerbExecCount * sizeof(u32)];
        }

        if (WordType == TYPE_INTEGER)
        {
            s32 Value = strtol(WordString, 0, 10);
            (*(s32*)(DefinitionData)) = Value;
            LOG("INT %d\n", Value);
        }
        else if(WordType == TYPE_FLOAT)
        {
            f32 Value = strtof(WordString, 0);
            (*(f32*)(DefinitionData)) = Value;
            LOG("FLOAT %f\n", Value);
        }
        else if (WordType == TYPE_NONE)
        {
            u64 VerbHash = XXH64(WordString, WordLength, 0xdeadc0de);
            s32 Verb = WordIsCoreVerb(VerbHash);
            if (Verb != VERB_NONE)
            {
                if (Verb == VERB_DEFINITION_START)
                {
                    if (VerbDefinitionStage != 0)
                    {
                        LOG("ERROR: Cannot define a verb inside another verb!\n");
                        break;
                    }
                    else
                    {
                        VerbDefinitionStage = 1;
                        continue;
                    }
                }
                else if (Verb == VERB_DEFINITION_END)
                {
                    if (VerbDefinitionStage == 0)
                    {
                        LOG("ERROR: Not defining a verb!\n");
                        break;
                    }
                    else if (VerbDefinitionStage == 1)
                    {
                        LOG("ERROR: Empty verb name!\n");
                        break;
                    }
                    else
                    {
                        Forth->VerbOffsetEntries[CurrentVerbEntry].StartOffset = CurrentVerbStartOffset;
                        Forth->VerbOffsetEntries[CurrentVerbEntry].WordCount = Forth->VerbExecCount - CurrentVerbStartOffset;
                        ++Forth->VerbsCount;
                        VerbDefinitionStage = 0;
                        continue;
                    }
                }
                else
                {
                    WordType = TYPE_CORE_VERB;
                    (*(u32*)(DefinitionData)) = Verb;
                    LOG("%s\n", CoreVerbNames[Verb]);
                }
            }

            // Word is not a builtin verb
            else
            {
                if (VerbDefinitionStage == 1)
                {
                    // TODO: Pass this word as it is the name of the verb definition
                    CurrentVerbStartOffset = Forth->VerbExecCount;
                    CurrentVerbEntry = FindVerbEntry(Forth, WordString, WordLength);
                    if (CurrentVerbEntry == 0xffffffff)
                    {
                        CurrentVerbEntry = Forth->VerbsCount;
                    }
                    LOG("Loading into verb: %.*s\n", WordLength, WordString);
                }
                else // 0/2
                {
                    WordType = TYPE_VERB;
                    u32 VerbEntry = FindVerbEntry(Forth, WordString, WordLength);
                    if (VerbEntry == 0xffffffff)
                    {
                        LOG("ERROR: Unknown verb %.*s\n", WordLength, WordString);
                    }
                    else
                    {
                        (*(u32*)(DefinitionData)) = VerbEntry;
                        LOG("USER VERB: %.*s\n", WordLength, WordString);
                    }
                }
            }
        }

        if (VerbDefinitionStage == 0)
        {
            Forth->ExecTypes[Forth->ExecCount] = WordType;
            Forth->ExecCount++;
        }
        else if (VerbDefinitionStage == 1)
        {
            u32* Data = (u32*)(&Forth->ExecData[Forth->ExecCount * sizeof(u32)]);
            *Data = CurrentVerbEntry;
            Forth->ExecTypes[Forth->ExecCount] = TYPE_VERB_DEFINITION_ENTRY;
            Forth->ExecCount++;
            *(Data+1) = CurrentVerbStartOffset;
            Forth->ExecTypes[Forth->ExecCount] = TYPE_VERB_DEFINITION_CODEPOINT;
            Forth->ExecCount++;
            VerbDefinitionStage = 2;
        }
        else if (VerbDefinitionStage == 2)
        {
            Forth->VerbExecTypes[Forth->VerbExecCount] = WordType;
            Forth->VerbExecCount++;
        }
    }

#if 0
    for (u32 ii=0; ii<Forth->VerbsCount; ++ii)
    {
        forth_verb_offset Verb = Forth->VerbOffsetEntries[ii];
        LOG("VERB %d\n" ,ii);
        char* VerbExecData = Forth->VerbExecData;
        for (u32 jj=0; jj<Verb.WordCount; ++jj)
        {
            u16 WordType = Forth->VerbExecTypes[jj+Verb.StartOffset];
            if (WordType == TYPE_INTEGER)
            {
                LOG("INT %d\n", *((s32*)&VerbExecData[(jj+Verb.StartOffset) * 4]));
            }
            else if(WordType == TYPE_FLOAT)
            {
                LOG("FLOAT %f\n", *((f32*)&VerbExecData[(jj+Verb.StartOffset) * 4]));
            }
        }
    }
#endif
}

#define Word0 Forth->Count-1
#define Word1 Forth->Count-2
#define Word2 Forth->Count-3
#define Word3 Forth->Count-4

void ExecuteForth(forth_ctx* Forth)
{
    int ExecCount = Forth->ExecCount;
    s32* Integers = (s32*)Forth->ExecData;
    f32* Floats   = (f32*)Forth->ExecData;
    s32* StackInt   = (s32*)Forth->Data;
    f32* StackFloat = (f32*)Forth->Data;
    u16* StackTypes = (u16*)Forth->DataTypes;
    for (int ii=0; ii<ExecCount; ++ii)
    {
        u16 WordType = Forth->ExecTypes[ii];
        switch (WordType)
        {
            case TYPE_INTEGER:
            {
                StackTypes[Forth->Count] = TYPE_INTEGER;
                StackInt[Forth->Count++] = Integers[ii];
                break;
            }

            case TYPE_FLOAT:
            {
                StackTypes[Forth->Count] = TYPE_FLOAT;
                StackFloat[Forth->Count++] = Floats[ii];
                break;
            }

            case TYPE_CORE_VERB:
            {
                u32 Verb = Integers[ii];
                switch (Verb)
                {
                    case VERB_NONE:
                    case VERB_DEFINITION_START:
                    case VERB_DEFINITION_END:
                        break; // Do nothing

                    case VERB_IADD:
                    {
                        s32 Result = StackInt[Word1] + StackInt[Word0];
                        StackInt[Word1] = Result;
                        StackTypes[Word1] = TYPE_INTEGER;
                        Forth->Count--;
                        break;
                    }
                    case VERB_ISUB:
                    {
                        s32 Result = StackInt[Word1] - StackInt[Word0];
                        StackInt[Word1] = Result;
                        StackTypes[Word1] = TYPE_INTEGER;
                        Forth->Count--;
                        break;
                    }
                    case VERB_IMUL:
                    {
                        s32 Result = StackInt[Word1] * StackInt[Word0];
                        StackInt[Word1] = Result;
                        StackTypes[Word1] = TYPE_INTEGER;
                        Forth->Count--;
                        break;
                    }
                    case VERB_IDIV:
                    {
                        s32 Result = StackInt[Word1] / StackInt[Word0];
                        StackInt[Word1] = Result;
                        StackTypes[Word1] = TYPE_INTEGER;
                        Forth->Count--;
                        break;
                    }
                    case VERB_FADD:
                    {
                        f32 Result = StackFloat[Word1] + StackFloat[Word0];
                        StackFloat[Word1] = Result;
                        StackTypes[Word1] = TYPE_FLOAT;
                        Forth->Count--;
                        break;
                    }
                    case VERB_FSUB:
                    {
                        f32 Result = StackFloat[Word1] - StackFloat[Word0];
                        StackFloat[Word1] = Result;
                        StackTypes[Word1] = TYPE_FLOAT;
                        Forth->Count--;
                        break;
                    }
                    case VERB_FMUL:
                    {
                        f32 Result = StackFloat[Word1] * StackFloat[Word0];
                        StackFloat[Word1] = Result;
                        StackTypes[Word1] = TYPE_FLOAT;
                        Forth->Count--;
                        break;
                    }
                    case VERB_FDIV:
                    {
                        f32 Result = StackFloat[Word1] / StackFloat[Word0];
                        StackFloat[Word1] = Result;
                        StackTypes[Word1] = TYPE_FLOAT;
                        Forth->Count--;
                        break;
                    }

                    default:
                    // Call verb function here

                    break;
                }
                break;
            }
        }
    }

    LOG("On stack after execution\n");
    for (u32 ii=0; ii<Forth->Count; ++ii)
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

    for (int ii=1; ii<ARRAY_SIZE(CoreVerbWords); ++ii)
    {
        CoreVerbHashes[ii] = XXH64(CoreVerbWords[ii], strlen(CoreVerbWords[ii]), 0xdeadc0de);
    }

    forth_ctx Forth = {0};
    InitForth(&Forth);
    LoadForth(&Forth, File.Contents, File.ContentsSize);
    ExecuteForth(&Forth);
    FreeForth(&Forth);

    return 0;
}
