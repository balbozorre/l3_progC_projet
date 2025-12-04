#ifndef CONFIG_H
#define CONFIG_H

// uncomment to use verbose mode
#define VERBOSE

#ifdef VERBOSE
    #define TRACE(...) fprintf(stderr, __VA_ARGS__);
    #define TRACE2(x,p1) fprintf(stderr, (x), (p1));
#else
    #define TRACE(...)
    #define TRACE1(x)
    #define TRACE2(x,p1)
#endif

#endif
