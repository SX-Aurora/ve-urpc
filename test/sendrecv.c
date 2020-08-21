#include <stdio.h>
#include <stdint.h>

#include "urpc.h"
#include "urpc_debug.h"

int pings = 0;
int pongs = 0;
int finish = 0;
int recvcmd = 0;
int shmid2 = 0;
int core2 = 0;
#define SEND_I  4
#define SEND_L  5
#define SEND_P  6
#define SEND_Q  7
#define SEND_Ix 8
#define SEND_D  9

#define RET_I  10
#define RET_L  11
#define RET_P  12
#define RET_Q  13
#define RET_Ix 14
#define RET_D  15

void send_ping_nolock(urpc_peer_t *up)
{
	urpc_mb_t m = { .c.cmd = 1, .c.offs=0, .c.len=0 };
	int64_t req = urpc_put_cmd(up, &m);
	if (req < 0)
		eprintf("send_ping failed\n");
}

void send_pong_nolock(urpc_peer_t *up)
{
	urpc_mb_t m = { .c.cmd = 2, .c.offs=0, .c.len=0 };
	int64_t req = urpc_put_cmd(up, &m);
	if (req < 0)
		eprintf("send_pong failed\n");
}

void send_exit_nolock(urpc_peer_t *up)
{
	urpc_mb_t m = { .c.cmd = 3, .c.offs=0, .c.len=0 };
	int64_t req = urpc_put_cmd(up, &m);
	if (req < 0)
		eprintf("send_exit failed\n");
}


int send_string_nolock(urpc_peer_t *up, char *s, uint32_t i, uint64_t l, char *fmt)
{
	int64_t rc;
	switch(fmt[0]){
	case 'I':
		if(fmt[1] == 'x'){
			rc = urpc_generic_send(up, SEND_Ix, fmt, i);
		}else{
			rc = urpc_generic_send(up, SEND_I, fmt, i);
		}
		break;
	case 'L':
		rc = urpc_generic_send(up, SEND_L, fmt, l);
		break;
	case 'P':
		rc = urpc_generic_send(up, SEND_P, fmt, s, (size_t)strlen(s));
		break;
	case 'Q':
		rc = urpc_generic_send(up, SEND_Q, fmt, s, (size_t)strlen(s));
		break;
	default:
		rc = urpc_generic_send(up, SEND_D, fmt, i);
		break;
	}
	if (rc < 0)
		eprintf("send_string failed\n");
	return rc;
}

static int ping_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                        void *payload, size_t plen)
{
	++pings;
	send_pong_nolock(up);
	return 0;
}

static int pong_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                        void *payload, size_t plen)
{
	++pongs;
	return 0;
}

static int exit_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                        void *payload, size_t plen)
{
	finish = 1;
	return 0;
}

static int string_handler_rcv(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          void *payload, size_t plen)
{
	uint32_t i;
	uint32_t i2;
	uint64_t l;
	char *p;
	size_t sz;
	char *p_tmp;

	switch(m->c.cmd){
	case SEND_I:
		urpc_unpack_payload(payload, plen, "I", &i);
		printf("buffer: '%u'\n", i);
		fflush(stdout);
                urpc_generic_send(up, RET_I, (char *)"I", i);

		break;
	case SEND_L:
		urpc_unpack_payload(payload, plen, "L", &l);
		printf("buffer: '%lu'\n", l);
		fflush(stdout);
                urpc_generic_send(up, RET_L, (char *)"L", l);

		break;
	case SEND_P:
		urpc_unpack_payload(payload, plen, "P", &p, &sz);
		printf("buffer size = %ld\n", sz);
		printf("buffer: '%s'\n", p+sz-7);
		printf("buffer addr: '%x'\n", p);
		fflush(stdout);
		p_tmp = p + sz -6;
                memset(p_tmp,'R',1);
                urpc_generic_send(up, RET_P, (char *)"P", p, sz);

		break;
	case SEND_Q:
		urpc_unpack_payload(payload, plen, "Q", &p, &sz);
		printf("buffer size = %ld\n", sz);
		printf("buffer: '%s'\n", p+sz-7);
		printf("buffer addr: '%x'\n", p);
		fflush(stdout);
                urpc_generic_send(up, RET_Q, (char *)"Q", p, sz);
		break;
	case SEND_Ix:
		urpc_unpack_payload(payload, plen, "Ix", &i);
		printf("buffer: '%u'\n", i);
		fflush(stdout);
                urpc_generic_send(up, RET_Ix, (char *)"Ix", i);

		break;
	case SEND_D:
		printf("buffer:Default'\n");
		fflush(stdout);
		i=2001;
                urpc_generic_send(up, RET_D, (char *)"D", i);

		break;
	}
	return 0;
}

static int string_handler_retrcv(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          void *payload, size_t plen)
{
        uint32_t i;
        uint32_t i2;
        uint64_t l;
        char *p;
        size_t sz;
        char *p_tmp;

        switch(m->c.cmd){
        case RET_I:
                urpc_unpack_payload(payload, plen, "I", &i);
                printf("ret buffer: '%u'\n", i);
		fflush(stdout);
                break;
        case RET_L:
                urpc_unpack_payload(payload, plen, "L", &l);
                printf("ret buffer: '%lu'\n", l);
		fflush(stdout);
                break;
        case RET_P:
                urpc_unpack_payload(payload, plen, "P", &p, &sz);
                printf("ret buffer size = %ld\n", sz);
                printf("ret buffer: '%s'\n", p+sz-7);
                printf("ret buffer addr: '%x'\n", p);
		fflush(stdout);
                break;
        case RET_Q:
                urpc_unpack_payload(payload, plen, "Q", &p, &sz);
                printf("ret buffer size = %ld\n", sz);
                printf("ret buffer: '%s'\n", p+sz-7);
                printf("ret buffer addr: '%x'\n", p);
		fflush(stdout);
                break;
        case RET_Ix:
                urpc_unpack_payload(payload, plen, "Ix", &i);
                printf("ret buffer: '%u'\n", i);
		fflush(stdout);
                break;
        case RET_D:
                urpc_unpack_payload(payload, plen, "D", &i);
		printf("buffer:Default'\n");
		fflush(stdout);
                break;
        }
        return 0;
}


static int receive_shmid_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          void *payload, size_t plen)
{
	uint32_t i;
	uint32_t i2;

		urpc_unpack_payload(payload, plen, "II", &i, &i2);
		dprintf("buffer: shmid '%d'\n", i);
		dprintf("buffer: core '%d'\n", i2);
		shmid2=i;
		core2=i2;
		recvcmd = 14;
	return 0;
}


void sendrecv_init(urpc_peer_t *up)
{
	int err;

	if ((err = urpc_register_handler(up, 1, &ping_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, 2, &pong_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, 3, &exit_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 3);
	if ((err = urpc_register_handler(up, SEND_I, &string_handler_rcv)) < 0)
		eprintf("register_handler failed for cmd %d\n", SEND_I);
	if ((err = urpc_register_handler(up, SEND_L, &string_handler_rcv)) < 0)
		eprintf("register_handler failed for cmd %d\n", SEND_L);
	if ((err = urpc_register_handler(up, SEND_P, &string_handler_rcv)) < 0)
		eprintf("register_handler failed for cmd %d\n", SEND_P);
	if ((err = urpc_register_handler(up, SEND_Q, &string_handler_rcv)) < 0)
		eprintf("register_handler failed for cmd %d\n", SEND_Q);
	if ((err = urpc_register_handler(up, SEND_Ix, &string_handler_rcv)) < 0)
		eprintf("register_handler failed for cmd %d\n", SEND_Ix);
        if ((err = urpc_register_handler(up, SEND_D, &string_handler_rcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", SEND_D);
        if ((err = urpc_register_handler(up, RET_I, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_I);
        if ((err = urpc_register_handler(up, RET_L, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_L);
        if ((err = urpc_register_handler(up, RET_P, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_P);
        if ((err = urpc_register_handler(up, RET_Q, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_Q);
        if ((err = urpc_register_handler(up, RET_Ix, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_Ix);
        if ((err = urpc_register_handler(up, RET_D, &string_handler_retrcv)) < 0)
                eprintf("register_handler failed for cmd %d\n", RET_D);

}

void sendrecv_init_err(urpc_peer_t *up)
{
	int err;
	int count = 0;
	
	for(;count < 257;count++){
		if ((err = urpc_register_handler(up, count, &ping_handler)) < 0)
			eprintf("register_handler failed for cmd %d\n", count);
	}
}
