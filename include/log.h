#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

extern bool verbose;

#define pr_info(fmt, ...) ({if (verbose) printf(fmt, ##__VA_ARGS__);})
#define pr_err(fmt, ...) dprintf(2, "Error(%s,%d) :" fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_perror(fmt, ...) pr_err(fmt ": %m\n", ##__VA_ARGS__)
