#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "forth.h"

#include "util.c"

#include "xxhash.h"

enum word_type
{
    TYPE_NONE,
    TYPE_BASIC_VERB,
    TYPE_VERB,
    TYPE_MODULE_VERB,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_VERB_DEFINITION_START, // Which entry we are replacing with
    TYPE_VERB_DEFINITION_END, 
};

#define BASIC_VERBS \
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
    XX(DUP, dup) \
    XX(SWAP, swap) \
    XX(OVER, over) \
    XX(DROP, drop) \
    XX(ROT, rot) \
    XX(RROT, -rot) \
    XX(PICK, pick) \
    XX(LOOP_START, do) \
    XX(LOOP_END, loop) \
    XX(LOOP_COUNTERI, I) \
    XX(LOOP_COUNTERF, .I) \
    XX(AUX_PUSH, >r) \
    XX(AUX_POP, r>) \
    XX(AUX_COPY_TOP, r@) \
    XX(DEFINITION_START, :) \
    XX(DEFINITION_END,   ;) \

#define XX(name, word) VERB_##name,
enum
{
    BASIC_VERBS
};
#undef XX

#define XX(name, word) #word,
static const char* BasicVerbWords[] =
{
    BASIC_VERBS
};
#undef XX

#define XX(name, word) "VERB_"#name,
static const char* BasicVerbNames[] =
{
    BASIC_VERBS
};
#undef XX
static u64 BasicVerbHashes[ARRAY_SIZE(BasicVerbNames)];

#include "mesh_module.cpp"

void InitForth(forth_ctx* Forth)
{
    Forth->ExecData  = (char*)malloc(Megabytes(1));
    Forth->ExecTypes = (u16*)malloc(Megabytes(1));
    Forth->VerbNameHashes = (u64*)malloc(Megabytes(1));
    Forth->VerbOffsetEntries = (forth_verb_offset*)malloc(Megabytes(1));
}

void FreeForth(forth_ctx* Forth)
{
    free(Forth->ExecData);
    free(Forth->ExecTypes);
    free(Forth->VerbNameHashes);
    free(Forth->VerbOffsetEntries);
}

#define min(a,b) a<b?a:b

u32 WordIsBasicVerb(u64 Hash)
{
    u32 Verb = 0;
    for (int ii=1; ii<ARRAY_SIZE(BasicVerbWords); ++ii)
    {
        u64 VerbHash = BasicVerbHashes[ii];
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

// Lookup verb with Hash in VerbNameHashes
u32 FindVerbEntry(forth_ctx* Forth, u64 Hash)
{
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

u32 FindModuleVerbEntry(forth_ctx* Forth, u64 Hash)
{
    u32 VerbsCount = Forth->ModuleVerbCount;
    u64* VerbHashes = Forth->ModuleVerbHashes;
    for (u32 ii=0; ii<VerbsCount; ++ii)
    {
        if (VerbHashes[ii] == Hash)
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
    u64 CurrentVerbHash = 0;

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
        void* DefinitionData = &Forth->ExecData[Forth->ExecCount * sizeof(u32)];

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

        if (WordType == TYPE_INTEGER)
        {
            s32 Value = strtol(WordString, 0, 10);
            (*(s32*)(DefinitionData)) = Value;
        }
        else if(WordType == TYPE_FLOAT)
        {
            f32 Value = strtof(WordString, 0);
            (*(f32*)(DefinitionData)) = Value;
        }

        else if (WordType == TYPE_NONE)
        {
            u64 VerbHash = XXH64(WordString, WordLength, FORTH_HASH_CONSTANT);
            s32 Verb = WordIsBasicVerb(VerbHash);
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
                        Forth->VerbNameHashes[CurrentVerbEntry] = CurrentVerbHash;
                        if (Forth->VerbsCount == CurrentVerbEntry)
                        {
                            Forth->VerbsCount++;
                        }
                        VerbDefinitionStage = 0;
                        Forth->ExecTypes[Forth->ExecCount++] = TYPE_VERB_DEFINITION_END;
                        Forth->ExecCount++;
                        continue;
                    }
                }

                else
                {
                    WordType = TYPE_BASIC_VERB;
                    (*(u32*)(DefinitionData)) = Verb;
                    //LOG("%s\n", BasicVerbNames[Verb]);
                }
            }

            // Word is not a core verb
            else
            {
                if (VerbDefinitionStage == 1)
                {
                    // TODO: Pass this word as it is the name of the verb definition
                    CurrentVerbEntry = FindVerbEntry(Forth, VerbHash);
                    if (CurrentVerbEntry == 0xffffffff)
                    {
                        CurrentVerbEntry = Forth->VerbsCount;
                    }
                    CurrentVerbHash = VerbHash;
                    //LOG("Loading into verb: %.*s\n", WordLength, WordString);
                }
                else // 0/2
                {
                    u32 VerbEntry = FindVerbEntry(Forth, VerbHash);
                    if (VerbEntry == 0xffffffff)
                    {
                        VerbEntry = FindModuleVerbEntry(Forth, VerbHash);
                        if (VerbEntry == 0xffffffff)
                        {
                            LOG("ERROR: Unknown verb %.*s\n", WordLength, WordString);
                        }
                        else
                        {
                            WordType = TYPE_MODULE_VERB;
                            (*(u32*)(DefinitionData)) = VerbEntry;
                            //LOG("MODULE VERB: %.*s\n", WordLength, WordString);
                        }
                    }
                    else
                    {
                        WordType = TYPE_VERB;
                        (*(u32*)(DefinitionData)) = VerbEntry;
                        LOG("USER VERB: %.*s\n", WordLength, WordString);
                    }
                }
            }
        }

        if (VerbDefinitionStage == 1)
        {
            (*(u32*)(DefinitionData)) = CurrentVerbEntry;
            Forth->ExecTypes[Forth->ExecCount] = TYPE_VERB_DEFINITION_START;
            Forth->ExecCount++;
            VerbDefinitionStage = 2;
        }
        else
        {
            Forth->ExecTypes[Forth->ExecCount] = WordType;
            Forth->ExecCount++;
        }
    }
}

#define Word0 Forth->Count-1
#define Word1 Forth->Count-2
#define Word2 Forth->Count-3
#define Word3 Forth->Count-4

void ExecuteForth(forth_ctx* Forth)
{
    u32 ExecCount = Forth->ExecCount;
    s32* Integers = (s32*)Forth->ExecData;
    f32* Floats   = (f32*)Forth->ExecData;
    s32* StackInt   = (s32*)Forth->Data;
    f32* StackFloat = (f32*)Forth->Data;
    u16* StackTypes = (u16*)Forth->DataTypes;
    u32* StackReturnStarts = (u32*)Forth->StackReturnStarts;
    u32* StackReturnEnds = (u32*)Forth->StackReturnEnds;
    u16* ExecTypes = Forth->ExecTypes;

    StackReturnEnds[0] = ExecCount;
    u32 ii = 0;
    for (;;)
    {
        if(StackReturnEnds[Forth->StackReturnCount] == ii)
        {
            if(Forth->StackReturnCount == 0)
            {
                break;
            }

            // Continue where we left off
            Forth->StackReturnCount--;
            ii = StackReturnStarts[Forth->StackReturnCount];
            continue;
        }

        u16 WordType = ExecTypes[ii];
        switch (WordType)
        {
            case TYPE_INTEGER:
            {
                StackTypes[Forth->Count] = TYPE_INTEGER;
                StackInt[Forth->Count++] = Integers[ii];
                //LOG("STACKING INT %d\n", Integers[ii]);
                break;
            }

            case TYPE_FLOAT:
            {
                StackTypes[Forth->Count] = TYPE_FLOAT;
                StackFloat[Forth->Count++] = Floats[ii];
                //LOG("STACKING FLOAT %f\n", Floats[ii]);
                break;
            }

            case TYPE_MODULE_VERB:
            {
                forth_fn VerbFunction = Forth->ModuleVerbFuncPtrs[Integers[ii]];
                VerbFunction(Forth, 0, 0);
                break;
            }
            case TYPE_VERB:
            {
                // Save program counter(ii) when jumping to a subroutine
                StackReturnStarts[Forth->StackReturnCount++] = ii+1;
                u32 Verb = Integers[ii];
                forth_verb_offset VerbOffset = Forth->VerbOffsetEntries[Verb];
                ii = VerbOffset.StartOffset;
                StackReturnEnds[Forth->StackReturnCount] = VerbOffset.EndOffset;
                //LOG("Executing verb %d : %d : %d\n", Verb, ii, VerbOffset.EndOffset);
                continue;
                break;
            }

            // We should see this only at the top
            case TYPE_VERB_DEFINITION_START:
            {
                s32 CurrentVerbEntry = Integers[ii];
                u32 CurrentVerbStartOffset = ii+1;
                //LOG("Passing verb definition %d\n", Integers[ii]);
                while (ExecTypes[ii] != TYPE_VERB_DEFINITION_END && ii < ExecCount)
                {
                    ii++;
                }
                Forth->VerbOffsetEntries[CurrentVerbEntry].StartOffset = CurrentVerbStartOffset;
                Forth->VerbOffsetEntries[CurrentVerbEntry].EndOffset = ii;
                break;
            }

            case TYPE_BASIC_VERB:
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
                    case VERB_DUP:
                    {
                        u32 Dup = StackInt[Word0];
                        StackInt[Word0+1] = Dup;
                        StackTypes[Word0+1] = StackTypes[Word0];
                        Forth->Count++;
                        break;
                    }
                    case VERB_SWAP:
                    {
                        s32 TmpVal  = StackInt[Word1];
                        u16 TmpType = StackTypes[Word1];
                        StackInt[Word1] = StackInt[Word0];
                        StackTypes[Word1] = StackTypes[Word0];
                        StackInt[Word0] = TmpVal;
                        StackTypes[Word0] = TmpType;
                        break;
                    }
                    case VERB_OVER:
                    {
                        s32 TmpVal  = StackInt[Word1];
                        u16 TmpType = StackTypes[Word1];
                        StackInt[Word0+1] = TmpVal;
                        StackTypes[Word0+1] = TmpType;
                        Forth->Count++;
                        break;
                    }
                    case VERB_DROP:
                    {
                        Forth->Count--;
                        break;
                    }
                    case VERB_ROT:
                    {
                        s32 Tmp0     = StackInt[Word0];
                        u16 Tmp0Type = StackTypes[Word0];
                        s32 Tmp1     = StackInt[Word1];
                        u16 Tmp1Type = StackTypes[Word1];
                        StackInt[Word0] = StackInt[Word2];
                        StackTypes[Word0] = StackTypes[Word2];
                        StackInt[Word1] = Tmp0;
                        StackTypes[Word1] = Tmp0Type;
                        StackInt[Word2] = Tmp1;
                        StackTypes[Word2] = Tmp1Type;
                        break;
                    }
                    case VERB_RROT:
                    {
                        s32 Tmp0     = StackInt[Word0];
                        u16 Tmp0Type = StackTypes[Word0];
                        s32 Tmp1     = StackInt[Word1];
                        u16 Tmp1Type = StackTypes[Word1];
                        StackInt[Word0] = Tmp1;
                        StackTypes[Word0] = Tmp1Type;
                        StackInt[Word1] = StackInt[Word2];
                        StackTypes[Word1] = StackTypes[Word2];
                        StackInt[Word2] = Tmp0;
                        StackTypes[Word2] = Tmp0Type;
                        break;
                    }
                    case VERB_PICK:
                    {
                        s32 Index = StackInt[Word0];
                        StackInt[Word0] = StackInt[Word0-Index-1];
                        StackTypes[Word0] = StackTypes[Word0-Index-1];
                        break;
                    }
                    case VERB_LOOP_START:
                    {
                        s32 Counter = StackInt[Word0];
                        s32 End = StackInt[Word1];
                        Forth->Count -= 2;
                        u32 StackLoopCount = Forth->StackLoopCount;
                        Forth->StackLoopRewinds[StackLoopCount] = ii;
                        Forth->StackLoopCounters[StackLoopCount] = Counter;
                        Forth->StackLoopEnds[StackLoopCount] = End;
                        Forth->StackLoopCount++;
                        break;
                    }
                    case VERB_LOOP_END:
                    {
                        u32 StackLoopCount = Forth->StackLoopCount-1;
                        s32 Counter = Forth->StackLoopCounters[StackLoopCount];
                        s32 End = Forth->StackLoopEnds[StackLoopCount];
                        if (End != Counter)
                        {
                            s32 Increment = End > Counter ? 1 : -1;
                            ii = Forth->StackLoopRewinds[StackLoopCount];
                            Forth->StackLoopCounters[StackLoopCount] += Increment;
                        }
                        else
                        {
                            --Forth->StackLoopCount;
                        }
                        break;
                    }
                    case VERB_LOOP_COUNTERI:
                    {
                        Forth->Count++;
                        StackInt[Word0] = Forth->StackLoopCounters[Forth->StackLoopCount-1];
                        StackTypes[Word0] = TYPE_INTEGER;
                        break;
                    }
                    case VERB_LOOP_COUNTERF:
                    {
                        Forth->Count++;
                        StackFloat[Word0] = (f32)Forth->StackLoopCounters[Forth->StackLoopCount-1];
                        StackTypes[Word0] = TYPE_FLOAT;
                        break;
                    }
                    case VERB_AUX_PUSH:
                    {
                        u32 Data = StackInt[Word0];
                        u16 Type = StackTypes[Word0];
                        Forth->StackAuxData[Forth->StackAuxCount] = Data;
                        Forth->StackAuxTypes[Forth->StackAuxCount++] = Type;
                        Forth->Count--;
                        break;
                    }
                    case VERB_AUX_POP:
                    {
                        u32 Data = Forth->StackAuxData[Forth->StackAuxCount-1];
                        u16 Type = Forth->StackAuxTypes[--Forth->StackAuxCount];
                        Forth->Count++;
                        StackInt[Word0] = Data;
                        StackTypes[Word0] = Type;
                        break;
                    }
                    case VERB_AUX_COPY_TOP:
                    {
                        u32 Data = Forth->StackAuxData[Forth->StackAuxCount-1];
                        u16 Type = Forth->StackAuxTypes[Forth->StackAuxCount-1];
                        Forth->Count++;
                        StackInt[Word0] = Data;
                        StackTypes[Word0] = Type;
                        break;
                    }

                    default:
                    // Call verb function here

                    break;
                }
                break;
            }
        }
        ++ii;
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

#define LOAD_MODULES \
    XX(Mesh)

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

    for (int ii=1; ii<ARRAY_SIZE(BasicVerbWords); ++ii)
    {
        BasicVerbHashes[ii] = XXH64(BasicVerbWords[ii], strlen(BasicVerbWords[ii]), FORTH_HASH_CONSTANT);
    }

    static forth_ctx Forth;
    InitForth(&Forth);

#define XX(module) \
    Forth.ModuleVerbCount += LoadModule_##module(Forth.ModuleVerbHashes + Forth.ModuleVerbCount, \
            Forth.ModuleVerbFuncPtrs + Forth.ModuleVerbCount);
    LOAD_MODULES
#undef XX

    LoadForth(&Forth, File.Contents, File.ContentsSize);
    LOG("------------\nExecution\n-------------\n");
    ExecuteForth(&Forth);
    FreeForth(&Forth);

    return 0;
}
