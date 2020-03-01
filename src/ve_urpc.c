/*

*/

#include <stdlib.h>
#include <unistd.h>
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


// The following variables are thread local because each Context Thread can be a peer!
//__thread int this_peer = -1;			// local peer ID
//__thread struct ve_udma_peer *udma_peer;	// this peer's UDMA comm struct

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
urpc_peer_t *ve_urpc_init(int segid, int core)
{
	int err = 0;
	char *e;
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
			eprintf("ERROR: env variable URPC_SHM_SEGID not found.\n");
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
        init_dma_handler(&up->send);
        init_dma_handler(&up->recv);

	// pinning to VE core must happen before initializing UDMA
	if (core < 0 && (e = getenv("URPC_VE_CORE")) != NULL) {
		core = atoi(e);
	}
	if (core >= 0)
		_pin_threads_to_cores(core);

	// Initialize DMA
	err = ve_dma_init();
	if (err) {
		eprintf("Failed to initialize DMA\n");
		return NULL;
	}
	char *buff_base;
	uint64_t buff_base_vehva;
	size_t align_64mb = 64 * 1024 * 1024;
	size_t buff_size = 2 * URPC_BUFF_LEN;
	buff_size = (buff_size + align_64mb - 1) & ~(align_64mb - 1);

	// allocate read and write buffers in one call
	posix_memalign((void **)&buff_base, align_64mb, buff_size);
	if (buff_base == NULL) {
		eprintf("VE: allocating urpc mirror buffer failed! buffsize=%lu\n", buff_size);
                errno = ENOMEM;
		return NULL;
	}
	dprintf("ve allocated buff at %p, size=%lu\n", buff_base, buff_size);
        // TODO: is this needed?
	//busy_sleep_us(1*1000*1000);
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

	return up;
}

void ve_urpc_fini(urpc_peer_t *up)
{
	int err;

	// unregister local buffer from DMAATB
	err = ve_unregister_mem_from_dmaatb(up->recv.mirr_data_vehva -
					    offsetof(transfer_queue_t, data));
	if (err)
		eprintf("VE: Failed to unregister local buffer from DMAATB\n");

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
       return err;
}

/*
  URPC progress function.

  Process at most 'ncmds' requests from the RECV communicator.
  Return number of requests processed, -1 if error
*/
int ve_urpc_recv_progress(urpc_peer_t *up, int ncmds, int maxinflight)
{
	int64_t req, dhq_req;
	int done = 0;
	urpc_mb_t m;
	int inflight;

#ifdef SYNCDMA
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
#else
	do {
		done = 0;
		inflight = MAX(dhq_in_flight(&up->recv), dhq_in_flight(&up->send));
		// recv part
		if (inflight < maxinflight) {
			for (int i = 0; i < maxinflight - inflight; i++) {
				req = urpc_get_cmd(up->recv.tq, &m);
				if (req < 0)
					break;
				dhq_req = dhq_cmd_in(&up->recv, &m, 1);
				if (dhq_req != req) {
					eprintf("call_handler sp send failed, req mismatch.\n");
					return -1;
				}
				done++;
			}
		}
		inflight = MAX(dhq_in_flight(&up->recv), dhq_in_flight(&up->send));
		done += dhq_dma_submit(&up->recv, 1);
		done += dhq_cmd_check_done(&up->recv, 1);
                done += dhq_cmd_handle(up, 1);
		// send part
		done += dhq_dma_submit(&up->send, 0);
		done += dhq_cmd_check_done(&up->send, 0);
		done += dhq_cmd_handle(up, 0);
#ifdef DEBUGMEM
                if (done)
			dhq_state(up);
#endif
	} while (done > 0);
#endif // SYNCDMA
	return done;
}

/*
  Progress loop with timeout.
*/
int ve_urpc_recv_progress_timeout(urpc_peer_t *up, int ncmds, int maxinflight, long timeout_us)
{
	long done_ts = 0;
	do {
		int done = ve_urpc_recv_progress(up, ncmds, maxinflight);
		if (done == 0) {
			if (done_ts == 0)
				done_ts = get_time_us();
		} else
			done_ts = 0;

	} while (done_ts == 0 || timediff_us(done_ts) < timeout_us);

}

void dhq_state(struct urpc_peer *up)
{
	dma_handler_t *dh;

	dh = &up->recv.dhq;
	printf("#########################################################\n");
	printf("# DHQ recv: req=%4ld in=%3d submit=%3d done=%3d out=%3d #\n",
	       dh->in_req, dh->in, dh->submit, dh->done, dh->out);
	dh = &up->send.dhq;
	printf("# DHQ send: req=%4ld in=%3d submit=%3d done=%3d out=%3d #\n",
	       dh->in_req, dh->in, dh->submit, dh->done, dh->out);
	printf("#########################################################\n");
}

