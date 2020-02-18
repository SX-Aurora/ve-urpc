#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>

#include "vh_shm.h"
#include "urpc_common.h"

static int urpc_num_peers = 0;
static urpc_peer_t *urpc_peers[URPC_MAX_PEERS];


urpc_peer_t *vh_urpc_peer_get(int peer_id)
{
	if (peer_id >= 0 && peer_id < urpc_num_peers)
		return urpc_peers[peer_id];
	return NULL;
}

static void vh_urpc_comm_init(urpc_comm_t *uc)
{
	for (int i = 0; i < URPC_LEN_MB; i++) {
		uc->mlist[i].u64 = 0;
		TQ_WRITE64(uc->tq->mb[i].u64, 0);
	}
	TQ_WRITE32(uc->tq->sender_flags, 0);
	TQ_WRITE32(uc->tq->receiver_flags, 0);
	TQ_WRITE64(uc->tq->last_put_req, -1);
	TQ_WRITE64(uc->tq->last_get_req, -1);
	uc->free_begin = 0;
	uc->free_end = URPC_DATA_BUFF_LEN;
        pthread_mutex_init(&uc->lock, NULL);
}

/*
  VH side UDMA RPC communication init.
  
  - allocate shm seg for one peer
  - initialize VH side peer structure
  
  Returns: peer_id (can be 0) or a negative number, in case of an error.
*/
int vh_urpc_peer_create(void)
{
	int rc = 0, i, peer_id;
	char *env, *mb_offs = NULL;
	urpc_peer_t *up;

	up = (urpc_peer_t *)malloc(sizeof(urpc_peer_t));
	if (!up) {
		eprintf("veo_urpc_peer_init: malloc peer struct failed.\n");
		return -ENOMEM;
	}
	memset(up, 0, sizeof(up));
	peer_id = urpc_num_peers++;
	urpc_peers[peer_id] = up;

	/* TODO: make key VE and core specific to avoid duplicate use of UDMA */
	up->shm_key = getpid() * URPC_MAX_PEERS + urpc_num_peers;
	up->shm_size = 2 * URPC_BUFF_LEN;
        /*
         * Allocate shared memory segment
         */
	up->shm_segid = _vh_shm_init(up->shm_key, up->shm_size, &up->shm_addr);
	if (up->shm_segid == -1) {
		rc = _vh_shm_fini(up->shm_segid, up->shm_addr);
		return rc ? rc : -ENOMEM;
	}

	//
	// Set up send communicator
	//
	up->send.tq = (transfer_queue_t *)up->shm_addr;
	vh_urpc_comm_init(&up->send);

        //
        // Set up recv communicator
        //
        up->recv.tq = (transfer_queue_t *)(up->shm_addr + URPC_BUFF_LEN);
	vh_urpc_comm_init(&up->recv);

        pthread_mutex_init(&up->lock, NULL);

	// initialize handler table
	for (int i = 0; i <= URPC_MAX_HANDLERS; i++)
		up->handler[i] = NULL;
	handler_init_hook_t hook = urpc_get_handler_init_hook();
	if (hook)
		hook(up);

	return peer_id;
}

int vh_urpc_peer_destroy(int peer_id)
{
	int rc;

	urpc_peer_t *up = vh_urpc_peer_get(peer_id);
        
	rc = _vh_shm_fini(up->shm_segid, up->shm_addr);
	if (rc) {
		eprintf("vh_shm_fini failed for peer %d, rc=%d\n", peer_id, rc);
		return rc;
	}
	free(up);
	urpc_peers[peer_id] = NULL;
	return 0;
}

/*
  Create a child process running the binary in the args. Create appropriate
  environment vars for the child process and store the pid of the new process.
  This process is supposed to be the remote peer process running on a VE and
  connecting to the VH that created the up.

  Return 0 if all went well, -errno if not.
 */
int vh_urpc_child_create(urpc_peer_t *up, char *binary,
                         int venode_id, int ve_core)
{
	int err;

	pid_t c_pid = fork();
	if (c_pid == 0) {
		// this is the child

		// set env vars
		char tmp[16];
		sprintf(tmp, "%d", up->shm_segid);
		setenv("URPC_SHM_SEGID", tmp, 1);

		sprintf(tmp, "%d", venode_id);
		setenv("VE_NODE_NUMBER", tmp, 1);
		if (ve_core >= 0) {
			sprintf(tmp, "%d", ve_core);
			setenv("URPC_VE_CORE", tmp, 1);
		}

		// exec binary
		extern char** environ;
		char *e;
		e = getenv("URPC_VE_BIN");
		if (e)
			binary = e;
		char *argv[] = { binary, (char *)0 };
		err = execve(binary, argv, environ);
		if (err) {
			perror("ERROR: execve");
			exit(errno);
		}

	} else if (c_pid > 0) {
		// this is the parent
		up->child_pid = c_pid;
	} else {
		// this is an error
		perror("ERROR vh_urpc_child_create");
		return -errno;
	}
	return 0;
}

int vh_urpc_child_destroy(urpc_peer_t *up)
{
	int rc = -ENOENT;

	if (up->child_pid > 0) {
		rc = kill(up->child_pid, SIGKILL);
		up->child_pid = -1;
	}
	return rc;
}
