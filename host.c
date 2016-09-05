#include <stdio.h>
#include "platform.h"

extern void dotForthExecute(void* BytecodeStream, void* cmdbuf);

int main(int argc, char** argv)
{
    static char BytecodeStream[Kilobytes(64)];
    static int CommandBuffer[128];
    load_file_result Result = LoadFileIntoMemory("bytecode", BytecodeStream, sizeof BytecodeStream);
    dotForthExecute(BytecodeStream, CommandBuffer);

    for (int ii=0; ii<2; ++ii)
    {
        printf("%d ", CommandBuffer[ii]);
    }
    return 0;
}
