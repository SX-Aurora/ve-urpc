#include <stdio.h>

#include "urpc_common.h"
#include "pingpong.h"

int main()
{
	int err;

        extern void pingpong_init(urpc_peer_t *);
        urpc_set_handler_init_hook(&pingpong_init);
	urpc_peer_t *up = ve_urpc_init(0, -1);

	if (up == NULL)
		return -1;

	urpc_set_receiver_flags(&up->recv, 1);

	printf("VE: set receiver flag to 1.\n");

	while (!finish) {
		int rc = ve_urpc_recv_progress(up, 3);
	}
	
	ve_urpc_fini(up);
	return 0;
}
