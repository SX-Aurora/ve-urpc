#include <stdio.h>
#include <stdint.h>

#include "urpc_common.h"

int pings = 0;
int pongs = 0;
int finish = 0;

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

void send_string_nolock(urpc_peer_t *up, char *s)
{
	int64_t rc = urpc_generic_send(up, 4, "P", s, (size_t)strlen(s));
	if (rc < 0)
		eprintf("send_string failed\n");
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

static int string_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                          void *payload, size_t plen)
{
	char *p;
	size_t sz;

	urpc_unpack_payload(payload, plen, "P", &p, &sz);
	dprintf("buffer size = %ld\n", sz);
	dprintf("buffer: '%s'\n", p);
	return 0;
}

void pingpong_init(urpc_peer_t *up)
{
	int err;

	if ((err = urpc_register_handler(up, 1, &ping_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, 2, &pong_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, 3, &exit_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 3);
	if ((err = urpc_register_handler(up, 4, &string_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 4);
}
