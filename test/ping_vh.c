#include <stdio.h>

#include "urpc.h"
#include "urpc_time.h"
#include "urpc_debug.h"
#include "pingpong.h"

void vh_print_shm_coords(urpc_peer_t *up)
{
	printf("Run the following command in another shell:\n");
	printf("env ");
	printf("URPC_SHM_SEGID=%d ", up->shm_segid);
	printf("./pong_ve\n");
}

int main(int argc, char *argv[])
{
	int err = 0;
	long ts, te;

	extern void pingpong_init(urpc_peer_t *);
	extern int finish;
	urpc_set_handler_init_hook(&pingpong_init);

	urpc_peer_t *up = vh_urpc_peer_create();
	if (up == NULL)
		return -1;

	// start VE peer
	err = vh_urpc_child_create(up, "./pong_ve", 0, -1);
	if (!err)
		printf("VH: VE peer created as pid %d\n", up->child_pid);

	//vh_print_shm_coords(up);

	urpc_wait_peer_attach(up);

	if (pthread_mutex_trylock(&up->lock) != 0) {
		eprintf("found mutex locked!?\n");
		return 0;
	}

	// wait for peer receiver to set flag to 1
	while(! (urpc_get_receiver_flags(&up->send) & 0x1) ) {
		busy_sleep_us(1000);
	}

	printf("peer receiver active!\n");

	char buff[] = "the quick brown fox jumps over the lazy dog. 0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
	int nloop = 1000000;
	ts = get_time_us();

        for (int i = 0; i < nloop; i++) {
		//send_ping_nolock(up);
		send_string_nolock(up, buff);
		vh_urpc_recv_progress(up, 5);
        }

	vh_urpc_recv_progress_timeout(up, 1, 100);

	te = get_time_us();

        send_exit_nolock(up);
	
	pthread_mutex_unlock(&up->lock);

	printf("%d reqs in %fs: %f us/req\n", nloop, (double)(te-ts)/1.e6, (double)(te-ts)/nloop);

	vh_urpc_peer_destroy(up);
	return 0;
}

