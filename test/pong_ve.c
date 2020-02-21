#include <stdio.h>

#include "urpc_common.h"
#include "pingpong.h"

int main()
{
	int err;

        extern void pingpong_init(urpc_peer_t *);
        urpc_set_handler_init_hook(&pingpong_init);
	int rc = ve_urpc_init(0, -1);

	if (rc)
		return rc;

        extern urpc_peer_t ve_local_up_;
        urpc_peer_t *up = &ve_local_up_;
        
	urpc_set_receiver_flags(&up->recv, 1);

	printf("VE: set receiver flag to 1.\n");

	while (!finish) {
		int rc = urpc_recv_progress(up, 3);
	}
	
	ve_urpc_fini();
	return 0;
}
