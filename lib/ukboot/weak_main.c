#include <errno.h>
#include <uk/essentials.h>
#include <uk/print.h>
#include <flexos/isolation.h>

/* Internal main */
int __weak main(int argc __unused, char *argv[] __unused)
{
	uk_pr_info("weak main() called. Executing RPC server.\n");
	flexos_vmept_rpc_server_loop(NULL);
	return -EINVAL;
}
