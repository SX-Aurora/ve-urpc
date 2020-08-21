#include <stdio.h>
#include <stdint.h>


#include "urpc.h"
#include "urpc_time.h"
#include "sendrecv.h"

int main(int argc, char *argv[])
{
	int err;

        char *buff;
        char *buff_tmp;
        int nloop =0;
        nloop = atoi(argv[1]);
        printf("loop cnt=%d\n",nloop);


        extern void sendrecv_init(urpc_peer_t *);
        urpc_set_handler_init_hook(&sendrecv_init);
	urpc_peer_t *up = ve_urpc_init(0);
	if (up == NULL)
		return -1;
        ve_urpc_init_dma(up, -1);

	urpc_set_receiver_flags(&up->recv, 1);
	printf("VE: set receiver flag to 1.\n");

        // wait for peer receiver to set flag to 1
        while(! (urpc_get_sender_flags(&up->send) & 0x1) ) {
               busy_sleep_us(1000);
        }
	printf("VE: get sender flag of send=%d\n",urpc_get_sender_flags(&up->send));
	printf("VE: get sender flag of recv=%d\n",urpc_get_sender_flags(&up->recv));
	printf("VE: get receiver flag of send=%d\n",urpc_get_receiver_flags(&up->send));
	printf("VE: get receiver flag of recv=%d\n",urpc_get_receiver_flags(&up->recv));


	int i;

        for (i = 0; i < nloop; ) {
          int rc = ve_urpc_recv_progress(up, 1);
          if (rc > 0){
		i++;
	  	printf("receive %d.\n",i);
          }
	}
	printf("VE: get sender flag of send=%d\n",urpc_get_sender_flags(&up->send));
	printf("VE: get sender flag of recv=%d\n",urpc_get_sender_flags(&up->recv));
	printf("VE: get receiver flag of send=%d\n",urpc_get_receiver_flags(&up->send));
	printf("VE: get receiver flag of recv=%d\n",urpc_get_receiver_flags(&up->recv));
        urpc_set_receiver_flags(&up->recv, 0);
        printf("VE: set reciever flag to 0.\n");

        // wait for peer sender to set flag to 1
        while( (urpc_get_sender_flags(&up->send) & 0x1) ) {
                busy_sleep_us(1000);
        }

	
	ve_urpc_fini(up);
	return 0;
}
