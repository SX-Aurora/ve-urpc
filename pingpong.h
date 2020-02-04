extern int pings;
extern int pongs;
extern int finish;

void send_ping_nolock(urpc_peer_t *up);
void send_pong_nolock(urpc_peer_t *up);
void send_exit_nolock(urpc_peer_t *up);
void send_string_nolock(urpc_peer_t *up, char *s);
