#ifndef URPC_COMMON_INCLUDE
#define URPC_COMMON_INCLUDE

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "urpc_debug.h"

/* maximum number of peer currently limited to 64 = 8 VEs * 10 cores */
#define URPC_MAX_PEERS 64
/* the length of the mailbox MUST be a power of 2! */
#define URPC_LEN_MB    256
#define URPC_BUFF_LEN (64 * 1024 * 1024)
#define URPC_CMD_BITS (8)

#define URPC_MAX_HANDLERS ((1 << URPC_CMD_BITS) - 1)

#define URPC_CMD_NONE (0)

#define URPC_PAYLOAD_BITS (27)
#define URPC_MAX_PAYLOAD (1 << URPC_PAYLOAD_BITS)
#define URPC_OFFSET_BITS (29)
#define URPC_DATA_BUFF_LEN (URPC_BUFF_LEN - 8*(URPC_LEN_MB + 2))

/* threshold to switch from lhm/shm to user DMA */
#define URPC_MIN_DMA_SIZE (16)
#define URPC_DELAY_PEEK 1
#define URPC_TIMEOUT_US (10 * 1000000)
#define URPC_ALLOC_TIMEOUT_US (60 * 1000000)

#define REQ2SLOT(r) (int32_t)((r) & (URPC_LEN_MB - 1))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ALIGN8B(x) (((uint64_t)(x) + 7UL) & ~7UL)

#ifdef __ve__
# define TQ_READ64(v) ve_inst_lhm((void *)&(v))
# define TQ_READ32(v) ve_inst_lhm32((void *)&(v))
# define TQ_WRITE64(var,val) ve_inst_shm((void *)&(var), val)
# define TQ_WRITE32(var,val) ve_inst_shm32((void *)&(var), val)
# define TQ_FENCE() ve_inst_fenceLSF()
#else
# define TQ_READ64(v) (v)
# define TQ_READ32(v) (v)
# define TQ_WRITE64(var,val) (var) = (val)
# define TQ_WRITE32(var,val) (var) = (val)
# define TQ_FENCE()
#endif

/*
  Communication buffer(s) layout in shared memory:

  Send buffer
  +-----------------
  | sender flags       : 32 bits
  +-----------------
  | receiver flags     : 32 bits
  +-----------------
  | sender req ID      : sender marks which req he last wrote. Slot is calculated from it
  +-----------------
  | read slot ID       : receiver marks which is the last read req. Slot is calculated from it
  +-----------------
  | cmd slot 0         : command slots
  | ...
  | cmd slot N-1
  +-----------------
  | data buffer        : contains payload buffers associated to each slot
  |
  |
  |
  +-----------------

  Receive buffer is a send buffer for the other peer. Only the roles are exchanged.

  Commands are written in round robin manner into the command slots. When no commands
  have been written, yet, the written slot ID contains a -1. Otherwise it points to
  the slot which has the last written command.

  When the reader (receiver side) reads a command slot, it writes its number into the
  "read slot ID" variable. Once the command has been processed, the receiver writes
  a zero into it. The sender can thus find out when the slot is available again.

  A command (in a slot) can be attached to a payload in the data buffer. The command
  uses 29 bits for the offset into the data buffer (if more space should be needed we
  could use 8-byte aligned buffers in which case we could actually use up to 32 bits,
  i.e. address 4GiB buffers by simply defining the offset as (offset / 8)). The command
  also contains a length field of up to 27 bits (128MiB). The payload should be
  transferrable by a single DMA transaction therefore must be less than 128MiB.

  The space in the payload data buffer must be managed by the sender. Like the slots, it
  is used in a round-robin fashion.
  

 */


#ifdef __cplusplus
extern "C" {
#endif

/*
  A mailbox slot for sending a command.
  The payload is in the data buffer.
  cmd  : the RPC command
  offs : offset inside payload data buffer, divided by 8 as data
         must be aligned anyway. This way we can address 512MiB
  len  : the length of the payload, max 128MiB
  
*/
union urpc_mb {
	uint64_t u64;
	struct {
		uint64_t  cmd : URPC_CMD_BITS;		// RPC command
		uint64_t offs : URPC_OFFSET_BITS;	// payload offset in buffer
		uint64_t  len : URPC_PAYLOAD_BITS;	// length of fragment
	} c;
};
typedef union urpc_mb urpc_mb_t;
	
struct transfer_queue {
	volatile uint32_t sender_flags;
	volatile uint32_t receiver_flags;
	volatile int64_t last_put_req;
	volatile int64_t last_get_req;
	volatile urpc_mb_t mb[URPC_LEN_MB];
	volatile uint64_t data[URPC_BUFF_LEN/sizeof(uint64_t) - URPC_LEN_MB - 2];
};
typedef struct transfer_queue transfer_queue_t;

union mlist {
	uint64_t u64;
	struct {
		uint32_t offs;
		uint32_t len;
	} b;
};
typedef union mlist mlist_t;

struct urpc_comm {
	// payload buffer memory management
	// memory block associated to each mailbox slot in transfer queue
	pthread_mutex_t lock;
	mlist_t mlist[URPC_LEN_MB];
	uint32_t free_begin;	// offset of beginning of free block
	uint32_t free_end;	// offset of end of free block
	transfer_queue_t *tq;	// communication buffer in shared memory segment
#ifdef __ve__
	uint64_t shm_data_vehva;	// start of payload buffer space in shm segment vehva
	uint64_t mirr_data_vehva;	// VEHVA address of VE mirror buffer to payload buffer
	void *mirr_data_buff;		// virtual address of VE mirror buffer
#endif
};
typedef struct urpc_comm urpc_comm_t;

struct urpc_peer;
typedef struct urpc_peer urpc_peer_t;

/*
  URPC handler function type.

  Arguments:
  urpc peer
  pointer to command/mailbox
  request ID
  pointer to payload buffer
  payload length
 */
typedef int (*urpc_handler_func)(urpc_peer_t *, urpc_mb_t *, int64_t, void *, size_t);
	
struct urpc_peer {
	urpc_comm_t send;
	urpc_comm_t recv;
	int shm_key, shm_segid;
	size_t shm_size;
	void *shm_addr;
	int shm_destroyed;
	pthread_mutex_t lock;
	pid_t child_pid;
	urpc_handler_func handler[256];
};

typedef void (*handler_init_hook_t)(urpc_peer_t *);

#ifdef __ve__

int ve_urpc_init(int segid, int core);
void ve_urpc_fini(void);
int ve_transfer_data_sync(uint64_t dst_vehva, uint64_t src_vehva, int len);

#else

urpc_peer_t *vh_urpc_peer_create(void);
int vh_urpc_peer_destroy(urpc_peer_t *up);
int vh_urpc_child_create(urpc_peer_t *up, char *binary,
                         int venode_id, int ve_core);
int vh_urpc_child_destroy(urpc_peer_t *up);

#endif

int wait_peer_attach(urpc_peer_t *up);
void urpc_set_receiver_flags(urpc_comm_t *uc, uint32_t flags);
void urpc_set_sender_flags(urpc_comm_t *uc, uint32_t flags);
uint32_t urpc_get_receiver_flags(urpc_comm_t *uc);
uint32_t urpc_get_sender_flags(urpc_comm_t *uc);
int64_t urpc_get_cmd(transfer_queue_t *tq, urpc_mb_t *m);
int64_t urpc_get_cmd_timeout(transfer_queue_t *tq, urpc_mb_t *m, long timeout_us);
int64_t urpc_put_cmd(urpc_peer_t *up, urpc_mb_t *m);
int64_t urpc_generic_send(urpc_peer_t *up, int cmd, char *fmt, ...);
void urpc_slot_done(transfer_queue_t *tq, int slot, urpc_mb_t *m);
int urpc_unpack_payload(void *payload, size_t psz, char *fmt, ...);
int urpc_recv_req_timeout(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          long timeout_us, void **payload, size_t *plen);
int urpc_recv_progress(urpc_peer_t *up, int ncmds);
int urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us);
int urpc_register_handler(urpc_peer_t *up, int cmd, urpc_handler_func handler);
void urpc_set_handler_init_hook(void (*func)(urpc_peer_t *up));
handler_init_hook_t urpc_get_handler_init_hook(void);

#ifdef __cplusplus
}
#endif

#endif /* URPC_COMMON_INCLUDE */
