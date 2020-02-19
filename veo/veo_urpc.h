#ifndef VEO_URPC_INCLUDE
#define VEO_URPC_INCLUDE

#include <urpc_common.h>

// reply timeout in us
#define REPLY_TIMEOUT 2000000

// maximum SEND/RECVFRAG transfer size
#define MAX_SENDFRAG ALIGN8B((URPC_DATA_BUFF_LEN * 8) / 10 )
// multipart SEND/RECVFRAG transfer size
#define PART_SENDFRAG ALIGN8B(URPC_DATA_BUFF_LEN >> 1)

#ifdef __cplusplus
extern "C" {
#endif

//
// URPC commands
//
enum veo_urpc_cmd
{
	URPC_CMD_NONE_     =  0,
	URPC_CMD_PING      =  1,
	URPC_CMD_EXIT      =  2,
	URPC_CMD_ACK       =  3, // ACK is a result with no (void) content
	URPC_CMD_RESULT    =  4, // result (int64_t) without cache
	URPC_CMD_RESCACHE  =  5, // result with cache
	URPC_CMD_LOADLIB   =  6, // load .so
	URPC_CMD_UNLOADLIB =  7, // unload .so
	URPC_CMD_GETSYM    =  8, // find symbol in .so
	URPC_CMD_ALLOC     =  9, // allocate buffer on VE
	URPC_CMD_FREE      = 10, // free buffer on VE
	URPC_CMD_SENDBUFF  = 11, 
	URPC_CMD_RECVBUFF  = 12,
	URPC_CMD_SENDFRAG  = 13,
	URPC_CMD_NEWPEER   = 14,
	URPC_CMD_CALL      = 15,
	URPC_CMD_SLEEPING  = 16  // notify peer that we're going to sleep
};

#if 0
	char *cmd_name[] = {
	"NONE",
	"PING",
	"EXIT",
	"ACK",
	"RESULT",
	"RESCACHE",
	"LOADLIB",
	"UNLOADLIB",
	"GETSYM",
	"ALLOC",
	"FREE",
	"SENDBUFF",
	"RECVBUFF",
	"SENDFRAG",
	"NEWPEER",
	"CALL",
	"SLEEPING"
	};
#endif

static inline int64_t send_cmd_nopayload(urpc_peer_t *up, enum veo_urpc_cmd cmd)
{
	urpc_mb_t m;
	m.c.cmd = cmd;
	m.c.offs=0; m.c.len=0;
	int64_t req = urpc_put_cmd(up, &m);
	if (req < 0)
		eprintf("sending cmd %d failed\n", cmd);
	return req;
}

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

 
extern int veo_finish_;

void send_ping_nolock(urpc_peer_t *up);
int64_t send_ack_nolock(urpc_peer_t *up);
int64_t send_result_nolock(urpc_peer_t *up, int64_t result);

int64_t send_read_mem_nolock(urpc_peer_t *up, uint64_t src, size_t size);
int64_t send_write_mem_nolock(urpc_peer_t *up, uint64_t dst, size_t size, void *buff, size_t bsz);

int wait_req_result(urpc_peer_t *up, int64_t req, int64_t *result);
int wait_req_ack(urpc_peer_t *up, int64_t req);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* VEO_URPC_INCLUDE */
