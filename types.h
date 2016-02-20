#include <stdint.h>
#include <stdbool.h>

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

#define f32 float 
#define f64 double 

#define UNUSED(expr) do { (void)(expr); } while (0)

#define Kilobytes(n) (n * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)

#define LOG printf
#define APP_EXPORT

#define ARRAY_SIZE(_ARR)      ((int)(sizeof(_ARR)/sizeof(*_ARR)))
