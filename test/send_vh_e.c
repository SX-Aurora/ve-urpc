#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "urpc.h"
#include "urpc_time.h"
#include "urpc_debug.h"
#include "sendrecv.h"

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

        char *buff;
        char *buff_tmp;
	int buflen =0;
	int nloop =0;
	
	if (argc < 6){
		printf("usage: cmd nloop format size ve_exe err_kind\n");
		exit(1);
	}
	nloop = atoi(argv[1]);
        printf("loop cnt=%d\n",nloop);

        char fmt[10];
	memset(fmt,'\0',10);
        strcpy(fmt,argv[2]);
        printf("fmt=%s\n",fmt);

	buflen = atoi(argv[3]);
        buff =malloc(buflen+1);
        buff_tmp = buff;
	memset(buff,'\0',buflen+1);
	strncpy(buff_tmp,"S:",2);
	buff_tmp += 2;
	memset(buff_tmp,'D',buflen - 4);
	buff_tmp += buflen - 4;
	strncpy(buff_tmp,":E",2);

        char ve_exe[10];
	memset(ve_exe,'\0',10);
        strcpy(ve_exe,argv[4]);
        printf("ve_exe=%s\n",ve_exe);

	int err_kind;
	err_kind = atoi(argv[5]);

        int len;
        len = strlen(buff);
	printf("buff len =%d\n",len);

	extern void sendrecv_init(urpc_peer_t *);
	extern void sendrecv_init_err(urpc_peer_t *);
	extern int finish;
	int hook_cnt=0;
	int hook_cnt_max=1;
	if (err_kind == 2){
		hook_cnt_max = 11;
	}
	for(;hook_cnt < hook_cnt_max;hook_cnt++){
		if (err_kind == 3){
			urpc_set_handler_init_hook(&sendrecv_init_err);
		}else{
			urpc_set_handler_init_hook(&sendrecv_init);
		}
	}

	urpc_peer_t *up = vh_urpc_peer_create();
	if (up == NULL)
		return -1;

	// start VE peer
        char cmd_str[20];
        memset(cmd_str,'\0',sizeof(cmd_str));
        sprintf(cmd_str,"%s %d %s %d",ve_exe, nloop, fmt, buflen);

	if (err_kind == 4){
		err = vh_urpc_child_create(up, cmd_str, 3, 20);
	}else{
		err = vh_urpc_child_create(up, cmd_str, 0, -1);
	}
	if (!err){
		printf("VH: VE peer created as pid %d\n", up->child_pid);
	}else{
		printf("VH: VE peer created err\n");
		//goto err1;
	}

	//vh_print_shm_coords(up);

	// Force become ERROR
	int shm_segid_sv;
	if (err_kind == 1){
		shm_segid_sv = up->shm_segid;
		up->shm_segid=9999;
	}
	err = urpc_wait_peer_attach(up);
	if (err_kind == 1){
		up->shm_segid=shm_segid_sv;
	}
	if (!err){
		printf("VH: VE peer attach pid %d\n", up->child_pid);
	}else{
                printf("VH: VE peer attach err retry\n");
                err = urpc_wait_peer_attach(up);
                if (!err){
                        printf("VH: VE peer attach pid %d\n", up->child_pid);
                }else{
                        printf("VH: VE peer attach err\n");
                        goto err1;
                }
	}

	if (pthread_mutex_trylock(&up->lock) != 0) {
		eprintf("found mutex locked!?\n");
		return 0;
	}



	// wait for peer receiver to set flag to 1
	while(! (urpc_get_receiver_flags(&up->send) & 0x1) ) {
		busy_sleep_us(1000);
	}

	printf("peer receiver active!\n");
        urpc_set_sender_flags(&up->recv, 1);
        urpc_get_sender_flags(&up->recv);
        printf("VH: get sender flag of send=%d\n",urpc_get_sender_flags(&up->send));
        printf("VH: get sender flag of recv=%d\n",urpc_get_sender_flags(&up->recv));
        printf("VH: get receiver flag of send=%d\n",urpc_get_receiver_flags(&up->send));
        printf("VH: get receiver flag of recv=%d\n",urpc_get_receiver_flags(&up->recv));



	ts = get_time_us();
	int send_ret;
	int rc;
	int send_err = 0;
	int i,j;
	uint32_t arg_i= 100;
	uint64_t arg_l= 200;
	int64_t next_slot=-1;

	if (err_kind == 3){
		urpc_unregister_handler(up, 257);
	}

        for (i = 0,j= 0; (i < nloop) || (j < nloop); ) {
		next_slot = urpc_next_send_slot(up);
		while(next_slot < 0){
                        printf("VH: sleep until next send slot become free %d\n",next_slot);
			sleep(1);
			next_slot = urpc_next_send_slot(up);
		}
		if (i < nloop){	
			i++;
                	printf("VH: send %d\n",i);
			buff_tmp = buff + (buflen - 5);
			sprintf(buff_tmp,"%03d:E",i);
			send_ret=send_string_nolock(up, buff, arg_i, arg_l, fmt);
			if(send_ret < 0){
				printf("VH: send error %d\n",send_ret);
				goto err1;
			}
		}
		if (j < nloop){
			rc = vh_urpc_recv_progress(up, 1); 
			if (rc > 0){
				j++;
                		printf("VH: recv %d\n",j);
			}
		}
		
        }
#if 0
	for (; j < i;) {
		rc = vh_urpc_recv_progress(up, 1);
		if (rc > 0){
			printf("VH: recv %d\n",j);
			j++;
		}
	}
#endif

        urpc_set_sender_flags(&up->recv, 0);
        urpc_set_receiver_flags(&up->recv, 1);
        printf("VH: get sender flag of send=%d\n",urpc_get_sender_flags(&up->send));
        printf("VH: get sender flag of recv=%d\n",urpc_get_sender_flags(&up->recv));
        printf("VH: get receiver flag of send=%d\n",urpc_get_receiver_flags(&up->send));
        printf("VH: get receiver flag of recv=%d\n",urpc_get_receiver_flags(&up->recv));

        // wait for peer sender to set flag to 1
        while( (urpc_get_receiver_flags(&up->send) & 0x1) ) {
                busy_sleep_us(1000);
        }

	vh_urpc_recv_progress_timeout(up, 1, 100);

	te = get_time_us();
err1:

        send_exit_nolock(up);
	vh_urpc_child_destroy(up);
	
	pthread_mutex_unlock(&up->lock);

	printf("%d reqs in %fs: %f us/req\n", nloop, (double)(te-ts)/1.e6, (double)(te-ts)/nloop);

	// Force become ERROR
	if (err_kind == 1){
		shm_segid_sv = up->shm_segid;
		up->shm_segid=9999;
	}
	int vupd_ret = 0;
	vupd_ret = vh_urpc_peer_destroy(up);
	if(vupd_ret < 0){
		printf("VH: vh_urpc_peer_destroy err %d\n",vupd_ret);
	}
	if (err_kind == 1){
		up->shm_segid=shm_segid_sv;
		vupd_ret = vh_urpc_peer_destroy(up);
		if(vupd_ret < 0){
			printf("VH: vh_urpc_peer_destroy err %d\n",vupd_ret);
		}
	}
	return 0;
}

