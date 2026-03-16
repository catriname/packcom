// ============================================================================
//  main.cpp  –  PackCom entry point
//  PackCom v0.75 Dev. – Packet Radio Terminal Program
//  Original by Jim Wright (WB4ZJV), 01/28/1987
//  Reconstructed in C++17 from compiled Turbo Pascal binaries
// ============================================================================
#include "app.h"
#include <cstdio>
#include <cstring>

// Usage banner (mirrors original /I command-line flag)
static void print_usage(const char* prog) {
    ::printf(
        "PackCom - Packet Radio Terminal\n"
        "Original by Jim Wright (WB4ZJV), v%s\n\n"
        "Usage: %s [/I]\n\n"
        "  /I   Re-initialize configuration to defaults (PACKCOM.CNF)\n\n"
        "Files used:\n"
        "  PACKCOM.CNF  - Configuration\n"
        "  PACKCOM.KEY  - Macro key definitions\n"
        "  PACKCOM.WRD  - Keyword scan list\n\n",
        PACKCOM_VERSION, prog
    );
}

int main(int argc, char* argv[]) {
    // Check for --help
    for (int i = 1; i < argc; ++i) {
        if (::strcmp(argv[i], "--help") == 0 || ::strcmp(argv[i], "-h") == 0 ||
            ::strcmp(argv[i], "/?") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    PackComApp app;
    return app.run(argc, argv);
}
