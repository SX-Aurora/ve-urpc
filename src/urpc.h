#ifndef URPC_H_INCLUDE
#define URPC_H_INCLUDE

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
 * Main include file with parameters, data types, exported function prototypes.
 * 
 * Copyright (c) 2020 Erich Focht
 */

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define MAX_VE_CORES   18
/* maximum number of peer currently limited to 128 = 8 VEs * 16 cores */
#define URPC_MAX_PEERS (8 * MAX_VE_CORES)
/* the length of the mailbox MUST be a power of 2! */
#define URPC_LEN_MB    256
#define URPC_BUFF_LEN_PER_THREADS (4 * 1024 * 1024)
#define URPC_CMD_BITS (8)

#define URPC_MAX_HANDLERS ((1 << URPC_CMD_BITS) - 1)

#define URPC_CMD_NONE (0)

#define URPC_PAYLOAD_BITS (27)
#define URPC_MAX_PAYLOAD (1 << URPC_PAYLOAD_BITS)
#define URPC_OFFSET_BITS (29)

#define URPC_DELAY_PEEK 1
#define URPC_TIMEOUT_US (10 * 1000000)
#define URPC_ALLOC_TIMEOUT_US (60 * 1000000)

#define ALIGN4B(x) (((uint64_t)(x) + 3UL) & ~3UL)
#define ALIGN8B(x) (((uint64_t)(x) + 7UL) & ~7UL)
#define REQ2SLOT(r) (int32_t)((r) & (URPC_LEN_MB - 1))

//
// Sender and receiver flags
//
#define URPC_FLAG_SLEEPING  1
#define URPC_FLAG_EXCEPTION 2
#define URPC_FLAG_EXITED    4

#ifdef __cplusplus
extern "C" {
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
	volatile uint64_t data[];
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

struct free_block {
	uint32_t begin;	// offset of beginning of free block
	uint32_t end;	// offset of end of free block
};
typedef struct free_block free_block_t;

struct urpc_comm {
	// payload buffer memory management
	// memory block associated to each mailbox slot in transfer queue
	pthread_mutex_t lock;
	mlist_t mlist[URPC_LEN_MB];
	free_block_t *active;	// active memory block
	free_block_t mem[2];	// free memory blocks
	transfer_queue_t *tq;	// communication buffer in shared memory segment
#ifdef __ve__
	uint64_t shm_data_vehva;	// start of payload buffer space in shm segment vehva
	uint64_t mirr_data_vehva;	// VEHVA address of VE mirror buffer to payload buffer
	void *mirr_data_buff;		// virtual address of VE mirror buffer
#endif
	int64_t data_buff_end;
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
#ifdef __ve__
	uint64_t shm_vehva;
	void *mirr_buff;
	int core;
#endif
	int shm_destroyed;
	pthread_mutex_t lock;
	pid_t child_pid;
	urpc_handler_func handler[256];
	int urpc_data_buff_len;
};
  
#ifdef __ve__

urpc_peer_t *ve_urpc_init(int segid);
int ve_urpc_init_dma(urpc_peer_t *up, int core);
void ve_urpc_unpin(void);
void ve_urpc_fini(urpc_peer_t *up);
int ve_urpc_recv_progress(urpc_peer_t *up, int ncmds);
int ve_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us);
void ve_prev_sent_payload(urpc_peer_t *up, int offs, void **payload, size_t *plen);

# else

urpc_peer_t *vh_urpc_peer_create(void);
int vh_urpc_peer_destroy(urpc_peer_t *up);
int vh_urpc_child_create(urpc_peer_t *up, char *binary,
                         int ve_node, int ve_core);
int vh_urpc_child_destroy(urpc_peer_t *up);
int vh_urpc_recv_progress(urpc_peer_t *up, int ncmds);
int vh_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us);

#endif

int set_recv_payload(urpc_comm_t *uc, urpc_mb_t *m, void **payload, size_t *plen);
int64_t urpc_generic_send(urpc_peer_t *up, int cmd, char *fmt, ...);
int64_t urpc_get_cmd(transfer_queue_t *tq, urpc_mb_t *m);
uint32_t urpc_get_receiver_flags(urpc_comm_t *uc);
uint32_t urpc_get_sender_flags(urpc_comm_t *uc);
int64_t urpc_next_send_slot(urpc_peer_t *up);
int64_t urpc_put_cmd(urpc_peer_t *up, urpc_mb_t *m);
int urpc_recv_req_timeout(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          long timeout_us, void **payload, size_t *plen);
int urpc_register_handler(urpc_peer_t *up, int cmd, urpc_handler_func h);
void urpc_set_handler_init_hook(void (*func)(urpc_peer_t *up));
void urpc_set_receiver_flags(urpc_comm_t *uc, uint32_t flags);
void urpc_set_sender_flags(urpc_comm_t *uc, uint32_t flags);
void urpc_slot_done(transfer_queue_t *tq, int slot, urpc_mb_t *m);
int urpc_unpack_payload(void *payload, size_t psz, char *fmt, ...);
int urpc_wait_peer_attach(urpc_peer_t *up);
int64_t urpc_max_send_cmd_size(urpc_peer_t *up);
#ifdef __cplusplus
}
#endif

#endif // URPC_H_INCLUDE
