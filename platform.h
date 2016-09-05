#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define f32 float 
#define f64 double 

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define s8  int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t

#define Kilobytes(n) (n * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)

#define LOG printf

#define ARRAY_SIZE(_ARR)      ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define ASSERT assert

#define max(a,b) ((a > b) ? a : b)
#define min(a,b) ((a < b) ? a : b)

typedef struct 
{
    char* Contents;
    u32 ContentsSize;
} load_file_result;


load_file_result 
LoadFileIntoMemory(const char *Filename, char* Memory, u32 MemorySize)
{
    load_file_result Result = {0};
    u32 Size = 0;
    FILE *File = fopen(Filename, "rb");
    if (File == NULL)
    {
        LOG("Couln't open %s\n", Filename);
        return Result;
    }
    fseek(File, 0, SEEK_END);
    Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    if (Size >= MemorySize)
    {
        LOG("Need larger memory for loading file %s\n", Filename);
        return Result;
    }
    char* Data = Memory;
    if (Size != fread(Data, sizeof(char), Size, File))
    {
        fclose(File);
        return Result;
    }
    fclose(File);

    Result.Contents = Data;
    Result.ContentsSize = Size;
    return Result;
}

#define Array(type, count, name) type name[count]; u32 name##Count;
#define AddArray(name) name[name##Count++]
#define CheckArray(cnt, name) if (name##Count < cnt)
#define CheckStaticArray(name) if (name##Count < ARRAY_SIZE(name))
#define ClearArray(name) name##Count = 0
#define ForArray(var, name) for (int var=0; var<name##Count; ++var)
#define ForSarray(var, name) for (int var=0; var<ARRAY_SIZE(name); ++var)
#define RemoveArray(name, idx) { if (name##Count > 0) { \
    if (name##Count == 1) { name##Count = 0;} else { memcpy(name + idx, name + idx + 1, name##Count - idx); name##Count--; } } }

#define iclamp(val, min, max) val = (val < min ? min : (val > max ? max : val))

#define DecrWrap(var, cnt) var = var == 0 ? cnt-1 : var-1
#define IncrWrap(var, cnt) var = (var + 1) % cnt
#define xorswp(a, b) a ^= b; b ^= a; a ^= b;
#define defstruct typedef struct 
