#ifndef URPC_COMMON_INCLUDE
#define URPC_COMMON_INCLUDE

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "urpc.h"
#include "urpc_debug.h"
#ifdef __ve__
#include <vedma.h>
//#include "dma_handler.h"
#endif

#define REQ2SLOT(r) (int32_t)((r) & (URPC_LEN_MB - 1))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ALIGN8B(x) (((uint64_t)(x) + 7UL) & ~7UL)

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
int ve_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us);

#else

int vh_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us);

#endif

int wait_peer_attach(urpc_peer_t *up);
int64_t urpc_get_cmd(transfer_queue_t *tq, urpc_mb_t *m);
int set_recv_payload(urpc_comm_t *uc, urpc_mb_t *m, void **payload, size_t *plen);
int64_t urpc_get_cmd_timeout(transfer_queue_t *tq, urpc_mb_t *m, long timeout_us);
int64_t urpc_next_send_slot(urpc_peer_t *up);
int64_t urpc_put_cmd(urpc_peer_t *up, urpc_mb_t *m);
void urpc_run_handler_init_hooks(urpc_peer_t *up);
uint64_t alloc_payload(urpc_comm_t *uc, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* URPC_COMMON_INCLUDE */
