#ifndef URPC_DEBUG_INCLUDE
#define URPC_DEBUG_INCLUDE

//#define DEBUG 1

#ifdef DEBUG

#ifdef __ve__
#define dprintf(args...) do { \
    fprintf(stdout, "[VE] " args);               \
    fflush(stdout);                              \
  } while(0)
#else
#define dprintf(args...) do { \
    fprintf(stdout, "[VH] " args);              \
    fflush(stdout);                             \
  } while(0)
#endif

#else

#define dprintf(args...)

#endif

#ifdef __ve__
#define eprintf(args...) do { \
    fprintf(stdout, "[VE] ERROR: " args);       \
    fflush(stdout);                             \
  } while(0)
#else
#define eprintf(args...) do { \
    fprintf(stdout, "[VH] ERROR: " args);       \
    fflush(stdout);                             \
  } while(0)
#endif

#endif /* URPC_DEBUG_INCLUDE */

