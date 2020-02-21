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

        uint64_t libh = veo_load_library(proc, "./libvehello.so");
        printf("libh = %p\n", (void *)libh);

        uint64_t sym = veo_get_sym(proc, libh, "hello");
        printf("sym = %p\n", (void *)sym);

        struct veo_args *argp = veo_args_alloc();

        veo_args_set_i64(argp, 0, 42);

        veo_args_free(argp);

        err = veo_proc_destroy(proc);
        printf("veo_proc_destroy() returned %d\n", err);

        return 0;
}

