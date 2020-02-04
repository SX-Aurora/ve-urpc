#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

//#include "veo_udma.h"

/*
  Returns: the segment ID of the shm segment.
 */
int _vh_shm_init(int key, size_t size, void **local_addr)
{
	int err = 0;
	struct shmid_ds ds;
	
	int segid = shmget(key, size, IPC_CREAT | SHM_HUGETLB | S_IRWXU); 
	if (segid == -1) {
		printf("[vh_shm_init] shmget failed: %s\n", strerror(errno));
		return -errno;
	}
	*local_addr = shmat(segid, NULL, 0);
	dprintf("local_addr: %p\n", *local_addr);
	if (*local_addr == (void *) -1) {
		printf("[vh_shm_init] shmat failed: %s\n"
			"Releasing shm segment. key=%d\n", strerror(errno), key);
		shmctl(segid, IPC_RMID, NULL);
		segid = -errno;
	}
	return segid;
}

int _vh_shm_fini(int segid, void *local_addr)
{
	int err = 0;

        dprintf("vh_shm_fini segid=%d\n", segid);
	if (local_addr != (void *)-1) {
		err = shmdt(local_addr);
		if (err < 0) {
			printf("[vh_shm_fini] Failed to detach from SHM segment %d at %p\n",
				segid, local_addr);
			return err;
		}
	}
	return err;
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

void vh_shm_wait_peers(int segid)
{
	struct shmid_ds ds;

	for (;;) {
		if (-1 == (shmctl(segid, IPC_STAT, &ds))) {
			perror("[vh_shm_wait_peers] Failed shmctl IPC_STAT");
			return;
		}
		if (ds.shm_nattch == 2)
			break;
	}
	_vh_shm_destroy(segid);
}
