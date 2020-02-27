#ifndef VEO_URPC_INCLUDE
#define VEO_URPC_INCLUDE

#include <urpc_common.h>

#ifndef __ve__
#include <CallArgs.hpp>
using veo::CallArgs;
#else
#include "ve_inst.h"
#endif

// reply timeout in us
#define REPLY_TIMEOUT 2000000

// multipart SEND/RECVFRAG transfer size
#define PART_SENDFRAG ALIGN8B(URPC_DATA_BUFF_LEN >> 3)

namespace veo {

//
// URPC commands
//
enum veo_urpc_cmd
{
	URPC_CMD_NONE_         =  0,
	URPC_CMD_PING          =  1,
	URPC_CMD_EXIT          =  2,
	URPC_CMD_ACK           =  3, // ACK is a result with no (void) content
	URPC_CMD_RESULT        =  4, // result (int64_t) without cache
	URPC_CMD_RESCACHE      =  5, // result with cache
        URPC_CMD_EXCEPTION     =  6, // notify about exception
	URPC_CMD_LOADLIB       =  7, // load .so
	URPC_CMD_UNLOADLIB     =  8, // unload .so
	URPC_CMD_GETSYM        =  9, // find symbol in .so
	URPC_CMD_ALLOC         = 10, // allocate buffer on VE
	URPC_CMD_FREE          = 11, // free buffer on VE
	URPC_CMD_SENDBUFF      = 12, 
	URPC_CMD_RECVBUFF      = 13,
	URPC_CMD_SENDFRAG      = 14,
	URPC_CMD_CALL          = 15, // simple call with no stack transfer
	URPC_CMD_CALL_STKIN    = 16, // call with stack "IN" only
	URPC_CMD_CALL_STKINOUT = 17, // call with stack IN and OUT
	URPC_CMD_SLEEPING      = 18, // notify peer that we're going to sleep
	URPC_CMD_NEWPEER       = 19  // create new remote peer (AKA context) inside same proc
};

enum veo_urpc_call_flags
{
	VEO_CALL_NO_STK  = 0,
	VEO_CALL_STK_IN  = 1,
	VEO_CALL_STK_OUT = 2
};

static inline int pickup_acks(urpc_peer_t *up, int acks)
{
	transfer_queue_t *tq = up->recv.tq;
	urpc_mb_t m;

	for (int i = 0; i < acks; i++) {
		int64_t req = urpc_get_cmd_timeout(tq, &m, REPLY_TIMEOUT);
		//int64_t req = urpc_get_cmd(tq, &m);
		if (req < 0) {
			eprintf("missing ACKs, expected %d, got %d\n", acks, i);
			return -1;
		}
		// mark slot as done, this is not handled by the receive loop
		urpc_slot_done(tq, REQ2SLOT(req), &m);
	}
	return 0;
}

 
void send_ping_nolock(urpc_peer_t *up);
int64_t send_ack_nolock(urpc_peer_t *up);
int64_t send_exception_nolock(urpc_peer_t *up, int64_t exc);

int64_t send_read_mem_nolock(urpc_peer_t *up, uint64_t src, size_t size);
int64_t send_write_mem_nolock(urpc_peer_t *up, uint64_t dst, size_t size, void *buff, size_t bsz);

int wait_req_result(urpc_peer_t *up, int64_t req, int64_t *result);
int wait_req_ack(urpc_peer_t *up, int64_t req);

#ifndef __ve__
int64_t send_call_nolock(urpc_peer_t *up, uint64_t ve_sp, uint64_t addr,
                         CallArgs &arg);
int unpack_call_result(urpc_mb_t *m, CallArgs *arg, void *payload, size_t plen,
                       uint64_t *result);
#endif

} // namespace veo

#endif /* VEO_URPC_INCLUDE */
