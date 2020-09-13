/**
 * TODO: add License header
 *
 * VE specific parts of VE-URPC.
 * 
 * (C)opyright 2020 Erich Focht
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <pthread.h>

#include <vhshm.h>
#include <vedma.h>

#include "urpc_common.h"
#include "urpc_time.h"


/*
  Initialize VH-SHM segment, map it as VEHVA.
*/
static int vhshm_register(urpc_peer_t *up)
{
	struct shmid_ds ds;
	uint64_t remote_vehva = 0;
	void *remote_addr = NULL;
	int err = 0;

	dprintf("VE: shm_segid = %d\n", up->shm_segid);

	//
	// attach shared memory VH address space and register it to DMAATB,
	// the region is accessible for DMA unter its VEHVA remote_vehva
	//
        up->shm_addr = vh_shmat(up->shm_segid, NULL, 0, (void **)&up->shm_vehva);
	if (up->shm_addr == NULL) {
		eprintf("VE: (shm_addr == NULL)\n");
		return -ENOMEM;
	}
	if (up->shm_vehva == (uint64_t)-1) {
		eprintf("VE: failed to attach to shm segment %d, shm_vehva=-1\n", up->shm_segid);
		return -ENOMEM;
	}
	return 0;
}

/**
 * @brief Pin the thread to a core. In case of OpenMP: pin all threads to consecutive cores.
 *
 * The VE side process must be pinned to a core because it is not allowed to change the core
 * and the DMA descriptor set.
 *
 * @param core the core of the main thread or thread #0 (for OpenMP)
 */
static void _pin_threads_to_cores(int core)
{
#ifdef _OPENMP
#pragma omp parallel
	{
		int thr = omp_get_thread_num();
		cpu_set_t set;
		memset(&set, 0, sizeof(cpu_set_t));
		set.__bits[0] = (1 << (thr + core));
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
	}
#else
	cpu_set_t set;
	memset(&set, 0, sizeof(cpu_set_t));
	set.__bits[0] = (1 << core);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
#endif
}

/**
 * @brief Unpin the thread(s).
 *
 * This function is needed when spawning a new pthread inside a VE handler (NEWPEER).
 * Without unpinning the new pthread inherits the parent's mask and is scheduled on
 * the same core, competing with the parent.
 *
 */
void ve_urpc_unpin(void)
{
	uint64_t mask = (1 << MAX_VE_CORES) - 1;
#ifdef _OPENMP
#pragma omp parallel
	{
		int thr = omp_get_thread_num();
		cpu_set_t set;
		memset(&set, 0, sizeof(cpu_set_t));
		set.__bits[0] = mask;
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
	}
#else
	cpu_set_t set;
	memset(&set, 0, sizeof(cpu_set_t));
        set.__bits[0] = mask;
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
#endif
}

static void ve_urpc_comm_init(urpc_comm_t *uc)
{
	uc->mem[0].begin = 0;
	uc->mem[0].end = DATA_BUFF_END;
	uc->mem[1].begin = 0;
	uc->mem[1].end = 0;
	uc->active = &uc->mem[0];
        pthread_mutex_init(&uc->lock, NULL);
}

// TODO: add pinning to a VE core!
urpc_peer_t *ve_urpc_init(int segid)
{
	int err = 0;
	char *e;

	long syscall(long n, ...);
	pid_t tid = syscall(SYS_gettid);
	if (getpid() != tid) {
		eprintf("You called ve_urpc_init() inside a forked/cloned thread.\n");
		eprintf("VE DMA registration must be called from the main thread!\n");
		return NULL;
	}
	urpc_peer_t *up = (urpc_peer_t *)malloc(sizeof(urpc_peer_t));
	if (up == NULL) {
		eprintf("unable to allocate urpc_peer struct memory\n");
                errno = ENOMEM;
		return NULL;
	}

        //
	// shm_segid is either in argument or in environment variable
	//
	if (segid) {
		up->shm_segid = segid;
	} else {
		if ((e = getenv("URPC_SHM_SEGID")) != NULL)
			up->shm_segid = atol(e);
		else {
			eprintf("env variable URPC_SHM_SEGID not found.\n");
			free(up);
			errno = ENOENT;
			return NULL;
		}
	}

	// find and register shm segment
	err = vhshm_register(up);
	if (err) {
		free(up);
		up = NULL;
		eprintf("VE: vh_shm_register failed, err=%d.\n", err);
		return NULL;
	}

	up->recv.tq = (transfer_queue_t *)(up->shm_vehva);
	up->send.tq = (transfer_queue_t *)(up->shm_vehva + URPC_BUFF_LEN);
        up->recv.shm_data_vehva = up->shm_vehva + offsetof(transfer_queue_t, data);
        up->send.shm_data_vehva = up->shm_vehva + URPC_BUFF_LEN
		+ offsetof(transfer_queue_t, data);

	ve_urpc_comm_init(&up->send);

	char *buff_base;
	uint64_t buff_base_vehva;
	size_t align_64mb = 64 * 1024 * 1024;
	size_t buff_size = 2 * URPC_BUFF_LEN;
	buff_size = (buff_size + align_64mb - 1) & ~(align_64mb - 1);

	// allocate read and write buffers in one call
	posix_memalign(&up->mirr_buff, align_64mb, buff_size);
	if (up->mirr_buff == NULL) {
		eprintf("VE: allocating urpc mirror buffer failed! buffsize=%lu\n", buff_size);
    	errno = ENOMEM;
		return NULL;
	}
	dprintf("ve allocated buff at %p, size=%lu\n", up->mirr_buff, buff_size);
        buff_base = (char *)up->mirr_buff;
	buff_base_vehva = ve_register_mem_to_dmaatb(buff_base, buff_size);
	if (buff_base_vehva == (uint64_t)-1) {
		eprintf("VE: mapping urpc mirror buffer failed! buffsize=%lu\n", buff_size);
                errno = ENOMEM;
		return NULL;
	}
	dprintf("ve_register_mem_to_dmaatb succeeded for %p\n", buff_base);

	up->recv.mirr_data_buff = buff_base + offsetof(transfer_queue_t, data);
	up->send.mirr_data_buff = buff_base + URPC_BUFF_LEN
		+ offsetof(transfer_queue_t, data);
	
	up->recv.mirr_data_vehva = buff_base_vehva + offsetof(transfer_queue_t, data);
	up->send.mirr_data_vehva = buff_base_vehva + URPC_BUFF_LEN
		+ offsetof(transfer_queue_t, data);

        // initialize handler table
	for (int i = 0; i <= URPC_MAX_HANDLERS; i++)
		up->handler[i] = NULL;
        urpc_run_handler_init_hooks(up);

	// don't remove this
	up->core = -1;
	return up;
}

/**
 * We need this split iof init because VE-SHM functions can only be done in the main thread.
 */
int ve_urpc_init_dma(urpc_peer_t *up, int core)
{
	char *e;

	// pinning to VE core must happen before initializing UDMA
	if (core < 0 && (e = getenv("URPC_VE_CORE")) != NULL) {
		core = atoi(e);
	}
	if (core >= 0)
		_pin_threads_to_cores(core);
	up->core = core;

	// Initialize DMA
	int err = ve_dma_init();
	if (err) {
		eprintf("Failed to initialize DMA\n");
		return -1;
	}
	return 0;
}

void ve_urpc_fini(urpc_peer_t *up)
{
	int err;

        // mark this side as exited.
        urpc_set_recv_flags(&up->recv, urpc_get_recv_flags(&up->recv) | URPC_FLAG_EXITED);
        urpc_set_send_flags(&up->send, urpc_get_send_flags(&up->send) | URPC_FLAG_EXITED);

	// unregister local buffer from DMAATB
	err = ve_unregister_mem_from_dmaatb(up->recv.mirr_data_vehva -
					    offsetof(transfer_queue_t, data));
	if (err)
		eprintf("VE: Failed to unregister local buffer from DMAATB\n");
        // free the mirror buffer
        free(up->mirr_buff);

	// detach VH sysV shm segment
	if (up->shm_addr) {
		err = vh_shmdt(up->shm_addr);
		if (err)
			eprintf("VE: Failed to detach from VH sysV shm\n");
		else {
			up->shm_addr = NULL;
			up->shm_vehva = 0;
		}
	}
	free(up);
}

/*
  Transfer buffer to from SHM area.

  The transfer length is smaller that the maximum transfer doable in one
  DMA descriptor (<128MiB).
*/
int ve_transfer_data_sync(uint64_t dst_vehva, uint64_t src_vehva, int len)
{
       int err;

       err = ve_dma_post_wait(dst_vehva, src_vehva, len);
       if (err) {
	       pid_t pid = getpid();
	       eprintf("DMA encountered exception, rc=0x%x\n", err);
	       if (err & 0x8000) {
		       eprintf("memory protection exception\n");
	       } else if (err & 0x4000) {
		       eprintf("memory protection exception\n");
	       } else if (err & 0x2000) {
		       eprintf("missing space exception\n");
	       } else if (err & 0x1000) {
		       eprintf("memory access exception\n");
	       } else if (err & 0x0800) {
		       eprintf("I/O access exception\n");
	       }
	       eprintf("Sleeping for 40s such that you can attach a debugger.\n");
	       eprintf("Command:  /opt/nec/ve/bin/gdb -p %d\n", pid);
	       sleep(40);
       }
       return err;
}

/*
  URPC progress function.

  Process at most 'ncmds' requests from the RECV communicator.
  Return number of requests processed, -1 if error
*/
int ve_urpc_recv_progress(urpc_peer_t *up, int ncmds)
{
	int64_t req, dhq_req;
	int done = 0;
	urpc_mb_t m;

	urpc_comm_t *uc = &up->recv;
	transfer_queue_t *tq = uc->tq;
        urpc_handler_func func = NULL;
        void *payload;
        size_t plen;
        int err;
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
int ve_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, long timeout_us)
{
	long done_ts = 0;
	do {
		int done = ve_urpc_recv_progress(up, ncmds);
		if (done == 0) {
			if (done_ts == 0)
				done_ts = get_time_us();
		} else
			done_ts = 0;

	} while (done_ts == 0 || timediff_us(done_ts) < timeout_us);
}

