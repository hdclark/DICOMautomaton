
#include <string>
#include "DCMA_Version.h"

#ifndef DCMA_VERSION
    #define DCMA_VERSION unknown
#endif
#ifndef DCMA_xSTR
    #define DCMA_xSTR(x) DCMA_STR(x)
#endif
#ifndef DCMA_STR
    #define DCMA_STR(x) #x
#endif

extern const std::string DCMA_VERSION_STR( DCMA_xSTR(DCMA_VERSION) );

#undef DCMA_VERSION
#undef DCMA_xSTR
#undef DCMA_STR

