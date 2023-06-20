#ifndef URPC_COMMON_INCLUDE
#define URPC_COMMON_INCLUDE

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
 * VH and VE commonly used macros.
 * 
 * Copyright (c) 2020 Erich Focht
 */

#include "urpc.h"
#include "urpc_debug.h"
#ifdef __ve__
#include <vedma.h>
//#include "dma_handler.h"
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifdef __ve__
# define TQ_READ64(v) ve_inst_lhm((void *)&(v))
# define TQ_READ32(v) ve_inst_lhm32((void *)&(v))
# define TQ_WRITE64(var,val) ve_inst_shm((void *)&(var), val)
# define TQ_WRITE32(var,val) ve_inst_shm32((void *)&(var), val)
#define TQ_FENCE() ve_inst_fenceLSF()
//# define TQ_FENCE() do {                        \
//	ve_inst_fenceSF();			\
//	ve_inst_fenceLF();			\
//	} while(0)
#define TQ_FENCE_L() ve_inst_fenceLF()
#define TQ_FENCE_S() ve_inst_fenceSF()

#else
# define TQ_READ64(v) (v)
# define TQ_READ32(v) (v)
# define TQ_WRITE64(var,val) (var) = (val)
# define TQ_WRITE32(var,val) (var) = (val)
# define TQ_FENCE()
# define TQ_FENCE_L()
# define TQ_FENCE_S()
#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __ve__
int ve_transfer_data_sync(uint64_t dst_vehva, uint64_t src_vehva, int len);
#endif

int64_t urpc_get_cmd_timeout(transfer_queue_t *tq, urpc_mb_t *m, long timeout_us);
void urpc_run_handler_init_hooks(urpc_peer_t *up);
uint64_t alloc_payload(urpc_comm_t *uc, uint32_t size);
#ifdef __cplusplus
}
#endif

#endif /* URPC_COMMON_INCLUDE */
