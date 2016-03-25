ForthVerb(SetMeshPosition)
{
    Pop_f32(zz);
    Pop_f32(yy);
    Pop_f32(xx);
    LOG("Mesh position: %f %f %f\n", xx, yy, zz);
}

#define MODULE_LOADER(name) u32 LoadModule_##name(u64* VerbHashes, forth_fn* VerbFuncPtrs)
#define MODULE_VERBS \
    XX(SetMeshPosition, mesh-pos) \

//
MODULE_LOADER(Mesh)
{
#define XX(name, word) #word,
    static const char* VerbWords[] =
    {
        MODULE_VERBS
    };
#undef XX
#define XX(name, word) _ForthVerb_##name,
    static forth_fn VerbFunctions[ARRAY_SIZE(VerbWords)] = 
    {
        MODULE_VERBS
    };
#undef XX

    for (int ii=0; ii<ARRAY_SIZE(VerbWords); ++ii)
    {
        VerbHashes[ii] = XXH64(VerbWords[ii], strlen(VerbWords[ii]), FORTH_HASH_CONSTANT);
    }

    memcpy(VerbFuncPtrs, VerbFunctions, sizeof(VerbFunctions));
    return ARRAY_SIZE(VerbWords);
}

#undef MODULE_VERBS
