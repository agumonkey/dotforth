struct forth_verb_offset
{
    u32 StartOffset;
    u32 EndOffset;
};

struct program_ctx;
struct forth_ctx;
typedef void (*forth_fn)(forth_ctx*, program_ctx*, void*);

struct forth_ctx
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

    u32 StackReturnStarts[32];
    u32 StackReturnEnds[32];
    u32 StackReturnCount;   

    // Stack
    u64    Data[64]; 
    u64    _Guard0[64]; 
    u16    DataTypes[64]; 
    u64    _Guard1[64]; 
    u64    Count;   // # of items on the stack

    forth_fn ModuleVerbFuncPtrs[512];
    u64      ModuleVerbHashes[512];
    u16      ModuleVerbCount;
};

#define ForthVerb(name) void _ForthVerb_##name(forth_ctx* _Forth, program_ctx *Ctx, void* UserData)

#define Pop_f32(var) f32 var = ((f32*)(_Forth->Data))[--_Forth->Count];
#define Pop_s32(var) s32 var = ((s32*)(_Forth->Data))[--_Forth->Count];

#define Push_f32(var) ((f32*)(_Forth->Data))[_Forth->Count++] = var;
#define Push_s32(var) ((s32*)(_Forth->Data))[--_Forth->Count++] = var;

#define FORTH_HASH_CONSTANT 0xdeadc0de

struct program_ctx
{

};

