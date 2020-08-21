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

int string_retrcv(urpc_peer_t *up, urpc_mb_t *m, int64_t req, void *payload, size_t plen)
{
        uint32_t i;
        uint64_t l;
        char *p;
        size_t sz;
        char *p_tmp;

        switch(m->c.cmd){
        case 9:
                urpc_unpack_payload(payload, plen, "I", &i);
                printf("ret buffer: '%u'\n", i);
		fflush(stdout);
                recvcmd = 9;
                break;
        case 10:
                urpc_unpack_payload(payload, plen, "L", &l);
                printf("ret buffer: '%lu'\n", l);
		fflush(stdout);
                recvcmd = 10;
                break;
        case 11:
                urpc_unpack_payload(payload, plen, "P", &p, &sz);
                printf("ret buffer size = %ld\n", sz);
                printf("ret buffer: '%s'\n", p+sz-7);
                printf("ret buffer addr: '%x'\n", p);
		fflush(stdout);
                recvcmd = 11;
                break;
        case 12:
                urpc_unpack_payload(payload, plen, "Q", &p, &sz);
                printf("ret buffer size = %ld\n", sz);
                printf("ret buffer: '%s'\n", p+sz-7);
                printf("ret buffer addr: '%x'\n", p);
		fflush(stdout);
                recvcmd = 12;
                break;
        case 13:
                urpc_unpack_payload(payload, plen, "Ix", &i);
                printf("ret buffer: '%u'\n", i);
		fflush(stdout);
                recvcmd = 13;
                break;
        }
        return 0;
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
		printf("usage: cmd nloop format size ve_exe recv_kind\n");
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

	int recv_kind;
	recv_kind = atoi(argv[5]);

        int len;
        len = strlen(buff);
	printf("buff len =%d\n",len);

	extern void sendrecv_init(urpc_peer_t *);
	extern int finish;
	urpc_set_handler_init_hook(&sendrecv_init);

	urpc_peer_t *up = vh_urpc_peer_create();
	if (up == NULL)
		return -1;

	// start VE peer
        char cmd_str[20];
        memset(cmd_str,'\0',sizeof(cmd_str));
        sprintf(cmd_str,"%s %d",ve_exe, nloop);

	err = vh_urpc_child_create(up, cmd_str, 0, -1);
	if (!err){
		printf("VH: VE peer created as pid %d\n", up->child_pid);
	}else{
		printf("VH: VE peer created err\n");
		//goto err1;
	}

	//vh_print_shm_coords(up);

	err = urpc_wait_peer_attach(up);
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
	uint32_t arg_i= 4294967295;
	uint64_t arg_l= 18446744073709551615;
	int64_t next_slot=-1;
        urpc_mb_t m;
        void *payload = NULL;
        size_t plen = 0;
        urpc_comm_t *uc = &up->recv;
        transfer_queue_t *tq = uc->tq;


        for (i = 0,j= 0; (i < nloop) || (j < nloop); ) {
        //for (i = 0,j= 0; (i < nloop) ; ) {
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
			switch(recv_kind){
			case 1:
				rc = vh_urpc_recv_progress(up, 1); 
				if (rc > 0){
					j++;
                			printf("VH: recv_1 %d\n",j);
				}
				break;
			case 2:
                        	rc = vh_urpc_recv_progress_timeout(up, 1, 60000);
                        	if (rc > 0){
                                	j++;
                                	printf("VH: recv_2 %d\n",j);
                        	}
				break;
			case 3:
                        	rc = urpc_recv_req_timeout(up, &m, j, 60000, &payload, &plen);
                        	if (rc){
                                	j++;
                                	string_retrcv(up, &m ,j ,payload, plen);
                                	printf("VH: recv_3 %d\n",j);
                        	}
				break;
			case 4:
                        	rc = urpc_get_cmd_timeout(tq,  &m, 60000);
                        	if (rc >= 0){
                                	j++;
                                	set_recv_payload(uc, &m, &payload, &plen);
                                	string_retrcv(up, &m ,j ,payload, plen);
                                	printf("VH: recv_4 %d\n",j);
                        	}
				break;
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
        urpc_unregister_handler(up, 1);
        

err1:

        send_exit_nolock(up);
	vh_urpc_child_destroy(up);
	
	pthread_mutex_unlock(&up->lock);

	printf("%d reqs in %fs: %f us/req\n", nloop, (double)(te-ts)/1.e6, (double)(te-ts)/nloop);

	int vupd_ret = -1;
	vupd_ret = vh_urpc_peer_destroy(up);
	if(vupd_ret < 0){
		printf("VH: vh_urpc_peer_destroy err %d\n",vupd_ret);
	}
	return 0;
}

