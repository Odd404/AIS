#ifndef AIS_DEBUG_
#define AIS_DEBUG_

#include <iostream>

#define aisDebugInfo(x)     std::cerr << x << std::endl;
#define aisDebugWarn(x)     std::cerr << x << std::endl;
#define aisDebugError(x)    std::cerr << x << std::endl;

#ifdef DEBUG

#endif

#endif
