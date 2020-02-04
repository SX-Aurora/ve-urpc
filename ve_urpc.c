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

int shm_key, shm_segid;
uint64_t shm_vehva = 0;		// VEHVA of remote shared memory segment
size_t shm_size = 0;		// remote shared memory segment size
void *shm_remote_addr = NULL;	// remote address

// The following variables are thread local because each Context Thread can be a peer!
//__thread int this_peer = -1;			// local peer ID
//__thread struct ve_udma_peer *udma_peer;	// this peer's UDMA comm struct

urpc_peer_t *up;

/*
  Initialize VH-SHM segment, map it as VEHVA.
*/
static int vhshm_register(int segid)
{
	struct shmid_ds ds;
	uint64_t remote_vehva = 0;
	void *remote_addr = NULL;
	int err = 0;

        shm_segid = segid;
	dprintf("VE: shm_segid = %d\n", shm_segid);

	//
	// attach shared memory VH address space and register it to DMAATB,
	// the region is accessible for DMA unter its VEHVA remote_vehva
	//
        shm_remote_addr = vh_shmat(shm_segid, NULL, 0, (void **)&shm_vehva);
	if (shm_remote_addr == NULL) {
		eprintf("VE: (shm_remote_addr == NULL)\n");
		return -ENOMEM;
	}
	if (shm_vehva == (uint64_t)-1) {
		eprintf("VE: failed to attach to shm segment %d, shm_vehva=-1\n", shm_segid);
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

// TODO: add pinning to a VE core!
int ve_urpc_init(int segid, int core)
{
	int err = 0;
	char *e;

	up = (urpc_peer_t *)malloc(sizeof(urpc_peer_t));
	if (up == NULL) {
		eprintf("VE: malloc failed for peer struct.\n");
		err = -ENOMEM;
	} else
		dprintf("ve allocated up=%p\n", (void *)up);

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
			return -ENOENT;
		}
	}

	// find and register shm segment, if not done, yet
	if (shm_vehva == 0) {
		err = vhshm_register(up->shm_segid);
		if (err) {
			free(up);
			up = NULL;
			eprintf("VE: vh_shm_register failed, err=%d.\n", err);
			return err;
		}
	}

	up->recv.tq = (transfer_queue_t *)(shm_vehva);
	up->send.tq = (transfer_queue_t *)(shm_vehva + URPC_BUFF_LEN);
        up->recv.shm_data_vehva = shm_vehva + offsetof(transfer_queue_t, data);
        up->send.shm_data_vehva = shm_vehva + URPC_BUFF_LEN
		+ offsetof(transfer_queue_t, data);

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
	if (hook)
		hook(up);

	return 0;
}

void ve_urpc_fini(void)
{
	int err;

	// unregister local buffer from DMAATB
	err = ve_unregister_mem_from_dmaatb(up->recv.mirr_data_vehva
					    - offsetof(transfer_queue_t, data));
	if (err)
		eprintf("VE: Failed to unregister local buffer from DMAATB\n");

	// detach VH sysV shm segment
	if (shm_remote_addr) {
		err = vh_shmdt(shm_remote_addr);
		if (err)
			eprintf("VE: Failed to detach from VH sysV shm\n");
		else {
			shm_remote_addr = NULL;
			shm_vehva = 0;
		}
	}
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
