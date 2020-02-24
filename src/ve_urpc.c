/*

*/

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <pthread.h>

#include <vhshm.h>
#include <vedma.h>

#include "urpc_common.h"

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
	if (up->shm__addr == NULL) {
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
	uc->free_begin = 0;
	uc->free_end = URPC_DATA_BUFF_LEN;
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
		return err;
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
		return -ENOMEM;
	}
	dprintf("ve allocated buff at %p\n", buff_base);
        // TODO: is this needed?
	//busy_sleep_us(1*1000*1000);
	buff_base_vehva = ve_register_mem_to_dmaatb(buff_base, buff_size);
	if (buff_base_vehva == (uint64_t)-1) {
		eprintf("VE: mapping urpc mirror buffer failed! buffsize=%lu\n", buff_size);
		return -ENOMEM;
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
	handler_init_hook_t hook = urpc_get_handler_init_hook();
	if (hook) {
		dprintf("running handler init hook\n");
		hook(up);
        }

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
