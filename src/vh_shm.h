#ifndef VEO_UDMA_VHSHM_INCLUDE
#define VEO_UDMA_VHSHM_INCLUDE

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
 * VH-side shared memory handling support functions headers.
 * 
 * Copyright (c) 2020 Erich Focht
 */

#include <sys/types.h>

int _vh_shm_init(int key, size_t size, void **local_addr);
int _vh_shm_fini(int segid, void *local_addr);
int vh_shm_wait_peers(pid_t pid, int segid);

#endif /* VEO_UDMA_VHSHM_INCLUDE */
