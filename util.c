struct load_file_result
{
    char* Contents;
    u32 ContentsSize;
};

load_file_result LoadFileIntoMemory(const char *Filename, char* Memory, u32 MemorySize)
{
    load_file_result Result = {0};
    size_t Size = 0;
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

