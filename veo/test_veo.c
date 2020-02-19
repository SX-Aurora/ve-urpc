#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "ve_offload.h"

int main(int argc, char *argv[])
{
	int err = 0;

        struct veo_proc_handle *proc;

        proc = veo_proc_create_static(0, "./test_ve");

        printf("proc = %p\n", (void *)proc);
        fflush(stdout);

        sleep(2);

        uint64_t libh = veo_load_library(proc, "./libvehello.so");
        printf("libh = %p\n", (void *)libh);
        fflush(stdout);

        sleep(2);

        uint64_t sym = veo_get_sym(proc, libh, "hello");
        printf("sym = %p\n", (void *)sym);
        fflush(stdout);

        sleep(2);

        uint64_t buff_ve;
#define BUFFSIZE (266*1024*1024)
        err = veo_alloc_mem(proc, &buff_ve, BUFFSIZE);
        printf("alloc returned err=%d, buff_ve = %p\n", err, (void *)buff_ve);
        fflush(stdout);

        sleep(2);

        char *buff_vh;
        buff_vh = (char *)malloc(BUFFSIZE);

        for (int i=0; i<1; i++) {
          err = veo_read_mem(proc, (void *)buff_vh, buff_ve, BUFFSIZE);
          printf("read_mem returned err=%d\n", err);
        }
        
        sleep(2);

        for (int i=0; i<1; i++) {
          err = veo_write_mem(proc, buff_ve, (void *)buff_vh, BUFFSIZE);
          printf("write_mem returned err=%d\n", err);
        }
        
        sleep(2);

        free(buff_vh);
        err = veo_free_mem(proc, buff_ve);
        printf("free returned err=%d\n", err);
        fflush(stdout);

        sleep(2);
        err = veo_proc_destroy(proc);
        printf("veo_proc_destroy() returned %d\n", err);

        return 0;
}

