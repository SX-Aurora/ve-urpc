#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "urpc_debug.h"
#include "urpc_time.h"

/*
  Returns: the segment ID of the shm segment.
 */
int _vh_shm_init(int key, size_t size, void **local_addr)
{
	int err = 0;
	struct shmid_ds ds;

	int segid = shmget(key, size, SHM_HUGETLB | S_IRWXU);
	if (segid == -1) {
		eprintf("[vh_shm_init] shmget failed: %s\n", strerror(errno));
		return -errno;
	}
	*local_addr = shmat(segid, NULL, 0);
	dprintf("[vh_shm_init] shm seg local_addr: %p\n", *local_addr);
	if (*local_addr == (void *) -1) {
		eprintf("[vh_shm_init] shmat failed: %s. "
			"Releasing shm segment. key=%d\n", strerror(errno), key);
		shmctl(segid, IPC_RMID, NULL);
		segid = -errno;
	}
	return segid;
}

static void _vh_shm_destroy(int segid)
{
	int err = 0;
	struct shmid_ds ds;

	if (-1 == (shmctl(segid, IPC_STAT, &ds)))
		perror("[vh_shm_destroy] Failed shmctl IPC_STAT");
	err = shmctl(segid, IPC_RMID, &ds);
	if (err < 0)
		printf("[vh_shm_destroy] Failed to mark SHM seg ID %d destroyed\n", segid);
}

int _vh_shm_fini(int segid, void *local_addr)
{
	int err = 0;

        dprintf("vh_shm_fini segid=%d\n", segid);
	if (local_addr != (void *)-1) {
		// make SURE the shm is destroyed
		_vh_shm_destroy(segid);
		err = shmdt(local_addr);
		if (err < 0) {
			printf("[vh_shm_fini] Failed to detach from SHM segment %d at %p\n",
				segid, local_addr);
			return err;
		}
	}
	return err;
}

int vh_shm_wait_peers(int segid)
{
	struct shmid_ds ds;
        long ts = get_time_us();
        int rc = 0;

	for (;;) {
		if (-1 == (shmctl(segid, IPC_STAT, &ds))) {
			perror("[vh_shm_wait_peers] Failed shmctl IPC_STAT");
			return -1;
		}
		if (ds.shm_nattch == 2)
			break;
                if (timediff_us(ts) > 50000000) {
			rc = -1;
			perror("[vh_shm_wait_peers] Timeout while waiting for peer.");
			break;
		}
	}
	_vh_shm_destroy(segid);
	return rc;
}
