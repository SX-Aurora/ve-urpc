#ifndef DMA_HANDLER_INCLUDE
#define DMA_HANDLER_INCLUDE

struct urpc_comm;
struct urpc_peer;
union urpc_mb;

void init_dma_handler(struct urpc_comm *);
int64_t dhq_cmd_in(struct urpc_comm *, union urpc_mb *, int);
int dhq_cmd_check_done(struct urpc_comm *, int);
int dhq_dma_submit(struct urpc_comm *, int);
int dhq_cmd_handle(struct urpc_peer *, int);
int dhq_in_flight(struct urpc_comm *uc);

#endif
