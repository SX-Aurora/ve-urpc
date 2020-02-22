#ifndef URPC_DEBUG_INCLUDE
#define URPC_DEBUG_INCLUDE

#define DEBUG 1

#ifdef DEBUG

#ifdef __ve__
#define dprintf(args...) do { \
  printf("[VE] " args); \
  fflush(stdout); \
  } while(0)
#else
#define dprintf(args...) do { \
  printf("[VH] " args); \
  fflush(stdout); \
  } while(0)
#endif

#else

#define dprintf(args...)

#endif

#ifdef __ve__
#define eprintf(args...) do { \
  fprintf(stderr, "[VE] " args); \
  fflush(stderr); \
  } while(0)
#else
#define eprintf(args...) do { \
  fprintf(stderr, "[VH] " args); \
  fflush(stderr); \
  } while(0)
#endif

#endif /* URPC_DEBUG_INCLUDE */

