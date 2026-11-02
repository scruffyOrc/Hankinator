#include "load_cell_sync.h"
#include <cassert>
#include <cstdio>
int main() {
    for(unsigned expected=0;expected<16;++expected) {
        for(unsigned ready=0;ready<16;++ready) {
            bool complete=expected!=0;
            for(unsigned ch=0;ch<4;++ch)
                if((expected&(1u<<ch))&&!(ready&(1u<<ch)))complete=false;
            assert(LoadCellSync::canRead(expected,ready)==complete);
            assert(LoadCellSync::missing(expected,ready)==(expected&~ready));
        }
    }
    puts("Load-cell synchronization gate: all 256 combinations passed");
}
