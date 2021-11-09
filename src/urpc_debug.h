#ifndef URPC_DEBUG_INCLUDE
#define URPC_DEBUG_INCLUDE

/**
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * Macros for debug output.
 * 
 * Copyright (c) 2020 Erich Focht
 */

//#define DEBUG 1
//#define DEBUGMEM
//#define SYNCDMA

#ifdef DEBUG

#ifdef __ve__
#define dprintf(fmt, ...) do {                \
    fprintf(stdout, "[VE] " fmt, ## __VA_ARGS__);    \
    fflush(stdout);                              \
  } while(0)
#else
#define dprintf(fmt, ...) do {                                       \
    fprintf(stdout, "[VH] " fmt, ## __VA_ARGS__);         \
    fflush(stdout);                             \
  } while(0)
#endif

#else

#define dprintf(args...)

#endif

#ifdef __ve__
#define eprintf(fmt, ...) do {               \
    fprintf(stdout, "[VE] ERROR: " fmt, ## __VA_ARGS__);        \
    fflush(stdout);                             \
  } while(0)
#else
#define eprintf(fmt, ...) do {                                        \
    fprintf(stdout, "[VH] ERROR: " fmt, ## __VA_ARGS__);              \
    fflush(stdout);                             \
  } while(0)
#endif

#endif /* URPC_DEBUG_INCLUDE */

