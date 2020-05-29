#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <wait.h>
#include <signal.h>

#include "vh_shm.h"
#include "urpc_common.h"
#include "urpc_time.h"

static int _urpc_num_peers = 0;
static struct sigaction __reaper_sa = {0};


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
	uc->mem[0].begin = 0;
	uc->mem[0].end = DATA_BUFF_END;
	uc->mem[1].begin = 0;
	uc->mem[1].end = 0;
	uc->active = &uc->mem[0];
        pthread_mutex_init(&uc->lock, NULL);
}

/*
  VH side UDMA RPC communication init.
  
  - allocate shm seg for one peer
  - initialize VH side peer structure
 
  Returns: urpc_peer pointer if successful, NULL if failed.
*/
urpc_peer_t *vh_urpc_peer_create(void)
{
	int rc = 0, i, peer_id;
	char *env, *mb_offs = NULL;

	if (_urpc_num_peers == URPC_MAX_PEERS) {
		eprintf("veo_urpc_peer_init: max number of urpc peers reached!\n");
		errno = -ENOMEM;
		return NULL;
	}

	urpc_peer_t *up = (urpc_peer_t *)malloc(sizeof(urpc_peer_t));
	if (!up) {
		eprintf("veo_urpc_peer_create: malloc peer struct failed.\n");
		errno = -ENOMEM;
		return NULL;
	}
	memset(up, 0, sizeof(up));

	/* TODO: make key VE and core specific to avoid duplicate use of UDMA */
	up->shm_key = IPC_PRIVATE;
	up->shm_size = 2 * URPC_BUFF_LEN;
	/*
	 * Allocate shared memory segment
	 */
	up->shm_segid = _vh_shm_init(up->shm_key, up->shm_size, &up->shm_addr);
	if (up->shm_segid < 0) {
		rc = _vh_shm_fini(up->shm_segid, up->shm_addr);
		errno = -ENOMEM;
		return NULL;
	}

	_urpc_num_peers++;

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
	urpc_run_handler_init_hooks(up);

	return up;
}

int vh_urpc_peer_destroy(urpc_peer_t *up)
{
	int rc = _vh_shm_fini(up->shm_segid, up->shm_addr);
	if (rc) {
          eprintf("vh_shm_fini failed for peer %p, rc=%d\n", (void *)up, rc);
		return rc;
	}
	free(up);
        _urpc_num_peers--;
	return 0;
}

static int argsexp(char *args, char **argv, int maxargs)
{
	int argc = 0;
	
	char *p2 = strtok(args, " ");
	while (p2 && argc < maxargs-1) {
		argv[argc++] = p2;
		p2 = strtok(0, " ");
	}
	argv[argc] = 0;
	return argc;
}

static void handle_sigchld(int sig)
{
	int saved_errno = errno;
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
	errno = saved_errno;
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
	struct stat sb;
	int maxargs = 64;
	char *argv[maxargs];

	// exec binary
	extern char** environ;
	char *e;
	e = getenv("URPC_VE_BIN");
	if (!e)
		e = binary;
        char *args = strdup(e);
	int nargs = argsexp(args, argv, maxargs);

	if (stat(argv[0], &sb) == -1) {
		perror("stat");
		return -ENOENT;
	}
#if 0
	// For an unknown reason, if we have the SIGCHLD reaper in place
	// and call system() several times, it will randomly return
	// an error or succeed.
	if (__reaper_sa.sa_handler == NULL) {
		__reaper_sa.sa_handler = &handle_sigchld;
		sigemptyset(&__reaper_sa.sa_mask);
		__reaper_sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
		if (sigaction(SIGCHLD, &__reaper_sa, 0) == -1) {
			perror("Could not register reaper sighandler");
			exit(1);
		}
	}
#endif
	pid_t p_pid = getpid();
	pid_t c_pid = fork();
	if (c_pid == 0) {
		// this is the child
		err = prctl(PR_SET_PDEATHSIG, SIGTERM);
		if (err == -1) { 
			perror("ERROR: prctl");
			_exit(errno);
		 }
		if (getppid() != p_pid)
			exit(1);
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

		err = execve(argv[0], argv, environ);
		if (err) {
			perror("ERROR: execve");
			_exit(errno);
		}
		/* Not Reached */
	}  else if (c_pid > 0) {
		// this is the parent
		free(args);
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
	int rc = 0;
	int status;

	if (up->child_pid > 0) {
		printf("Sending SIGKILL to child");
		rc = kill(up->child_pid, SIGKILL);
		waitpid(up->child_pid, &status, 0);
		dprintf("waitpid(%d) returned status=%d\n", up->child_pid, status);
		up->child_pid = -1;
	}
	return rc;
}

int vh_urpc_recv_progress(urpc_peer_t *up, int ncmds)
{
	urpc_comm_t *uc = &up->recv;
	transfer_queue_t *tq = uc->tq;
	urpc_handler_func func = NULL;
	int err = 0, done = 0;
	urpc_mb_t m;
	void *payload = NULL;
	size_t plen = 0;

	while (done < ncmds) {
		int64_t req = urpc_get_cmd(tq, &m);
		if (req < 0)
			break;
		//
		// set/receive payload, if needed
		//
		set_recv_payload(uc, &m, &payload, &plen);
		//
		// call handler
		//
		func = up->handler[m.c.cmd];
		if (func) {
			err = func(up, &m, req, payload, plen);
			if (err)
				eprintf("Warning: RPC handler %d returned %d\n",
					m.c.cmd, err);
		}

		urpc_slot_done(tq, REQ2SLOT(req), &m);
		++done;
	}
	return done;
}

/*
  Progress loop with timeout.
*/
int vh_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us)
{
	long done_ts = 0;
	do {
		int done = vh_urpc_recv_progress(up, ncmds);
		if (done == 0) {
			if (done_ts == 0)
				done_ts = get_time_us();
		} else
			done_ts = 0;

	} while (done_ts == 0 || timediff_us(done_ts) < timeout_us);

}
