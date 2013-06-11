#ifndef _ARG_H_
#define _ARG_H_

class ArgDesc {
public:
    int kind;
    const char* key;
    int position;
    int type;
    union {
        int* asint;
        void** asptr;
        void* vptr;
    } dest;
    int reqd;

    static int const Position = 0;
    static int const Flag = 6;
    static int const END = 1;
    static int const Pointer = 2;
    static int const Int = 5;
    static int const Required = 3;
    static int const Optional = 4;
    static char* progname;
    static int verbose;

    ArgDesc(int ki, const char* ke, int po, int ty, void* v, int re) : kind(ki), key(ke), position(po), type(ty), reqd(re) { dest.vptr = v; }

    static void procargs(int argc, char** argv, ArgDesc* desc);
};

#endif

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

