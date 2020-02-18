#ifndef URPC_DEBUG_INCLUDE
#define URPC_DEBUG_INCLUDE

//#define DEBUG 1
#ifdef DEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(args...)
#endif
#define eprintf(args...) fprintf(stderr, args)

#endif /* URPC_DEBUG_INCLUDE */

