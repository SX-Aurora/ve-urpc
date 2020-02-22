#ifndef VEO_UDMA_VHSHM_INCLUDE
#define VEO_UDMA_VHSHM_INCLUDE

int _vh_shm_init(int key, size_t size, void **local_addr);
int _vh_shm_fini(int segid, void *local_addr);
int vh_shm_wait_peers(int segid);

#endif /* VEO_UDMA_VHSHM_INCLUDE */
