extern int pings;
extern int pongs;
extern int finish;
extern int recvcmd;
extern int shmid2;
extern int core2;

void send_ping_nolock(urpc_peer_t *up);
void send_pong_nolock(urpc_peer_t *up);
void send_exit_nolock(urpc_peer_t *up);
int send_string_nolock(urpc_peer_t *up, char *s, uint32_t i, uint64_t l, char *fmt);
