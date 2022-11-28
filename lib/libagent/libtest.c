#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	OCF_SUCCESS		0
#define	OCF_ERR_GENERIC		1
#define	OCF_ERR_ARGS		2
#define	OCF_ERR_UNIMPLEMENTED	3
#define	OCF_ERR_PERM		4
#define	OCF_ERR_INSTALLED	5
#define	OCF_ERR_CONFIGURED	6
#define	OCF_NOT_RUNNING		7

static const char     START_CMD[]       = "start";
static const char     STOP_CMD[]        = "stop";
static const char     STATUS_CMD[]      = "status";
static const char     MONITOR_CMD[]     = "monitor";
static const char     META_DATA_CMD[]   = "meta-data";

const char my_interp[] __attribute__((section(".interp"))) = "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2";

int
start(void) {
    static const char mes[] = "Hello from 'Start action' test agent.\n";
    int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

    write(fd, mes, sizeof(mes));
    close(fd);

    return OCF_SUCCESS;
}

int
stop(void) {
    static const char mes[] = "Hello from 'Stop action' test agent.\n";
    int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

    write(fd, mes, sizeof(mes));
    close(fd);

    return OCF_SUCCESS;
}

int
status(void) {
    static const char mes[] = "Hello from 'Status action' test agent.\n";
    int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

    write(fd, mes, sizeof(mes));
    close(fd);

    return OCF_SUCCESS;
}

int
monitor(void) {
    static const char mes[] = "Hello from 'Monitor action' test agent.\n";
    //int fd = open("/var/log/test.log", O_WRONLY | O_APPEND | O_CREAT, 0666);
    int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

    write(fd, mes, sizeof(mes));
    close(fd);

    return OCF_SUCCESS;
}

int
metadata(int stdout, int stderr) {
    static const char mes[] = "Hello from 'Metadata action' test agent.\n";
    int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

    write(fd, mes, sizeof(mes));
    close(fd);

    static const char meta_data[]=
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE resource-agent SYSTEM \"ra-api-1.dtd\">\n"
    "<resource-agent name=\"libtest.so\" version=\"1.0\">\n"
    "  <version>1.0</version>\n"
    "  <longdesc lang=\"en\">\n"
    "   This script is used to write data to a file.\n"
    "  </longdesc>\n"
    "  <shortdesc lang=\"en\">Test agent</shortdesc>\n"
    "  <actions>\n"
    "    <action name=\"start\"   timeout=\"15s\" />\n"
    "    <action name=\"stop\"    timeout=\"15s\" />\n"
    "    <action name=\"status\"  timeout=\"15s\" interval=\"15s\" />\n"
    "    <action name=\"monitor\" timeout=\"15s\" interval=\"15s\" />\n"
    "    <action name=\"meta-data\"  timeout=\"5s\" />\n"
    "  </actions>\n"
    "</resource-agent>\n";

    write(stdout, meta_data, sizeof(meta_data));
    return OCF_SUCCESS;
}

int
handler(int out_fd, int err_fd, char *action) {
    int ret = 0;
    if (strcmp(action, START_CMD) == 0){
        ret = start();
    } else if (strcmp(action, STOP_CMD) == 0) {
        ret = stop();
    } else if (strcmp(action, STATUS_CMD) == 0) {
        ret = status();
    } else if (strcmp(action, MONITOR_CMD) == 0) {
        ret = monitor();
    } else if (strcmp(action, META_DATA_CMD) == 0) {
        ret = metadata(out_fd, err_fd);
    }

    return ret;
}

int
mmain() {
    // return code
    int ret = 0;
    // is used to get parameters
    char *stack_address = __builtin_frame_address(0);
    // skip 24 bytes: ret address, argc, exec name
    char **argv = stack_address + 0x18;
    // parse action
    if (strcmp(*argv, START_CMD) == 0){
        ret = start();
    } else if (strcmp(*argv, STOP_CMD) == 0) {
        ret = stop();
    } else if (strcmp(*argv, STATUS_CMD) == 0) {
        ret = status();
    } else if (strcmp(*argv, MONITOR_CMD) == 0) {
        ret = monitor();
    } else if (strcmp(*argv, META_DATA_CMD) == 0) {
        ret = metadata(1, 2);
    }

    _exit(ret);
}

// #include <stdarg.h>
// #include <stdint.h>
// #include <string.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>

// #define OCF_SUCCESS             0
// #define OCF_ERR_GENERIC         1
// #define OCF_ERR_ARGS            2
// #define OCF_ERR_UNIMPLEMENTED   3
// #define OCF_ERR_PERM            4
// #define OCF_ERR_INSTALLED       5
// #define OCF_ERR_CONFIGURED      6
// #define OCF_NOT_RUNNING         7

// static const char     START_CMD[]       = "start";
// static const char     STOP_CMD[]        = "stop";
// static const char     STATUS_CMD[]      = "status";
// static const char     MONITOR_CMD[]     = "monitor";
// static const char     META_DATA_CMD[]   = "meta-data";

// const char my_interp[] __attribute__((section(".interp"))) = "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2";


// int
// start(void) {
//     static const char mes[] = "Hello from 'Start action' test agent.\n";
//     int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

//     write(fd, mes, sizeof(mes));
//     close(fd);

//     return OCF_SUCCESS;
// }

// int
// stop(void) {
//     static const char mes[] = "Hello from 'Stop action' test agent.\n";
//     int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

//     write(fd, mes, sizeof(mes));
//     close(fd);

//     return OCF_SUCCESS;
// }

// int
// status(void) {
//     static const char mes[] = "Hello from 'Status action' test agent.\n";
//     int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

//     write(fd, mes, sizeof(mes));
//     close(fd);

//     return OCF_SUCCESS;
// }

// int
// monitor(void) {
//     static const char mes[] = "Hello from 'Monitor action' test agent.\n";
//    // int fd = open("/var/log/test.log", O_WRONLY | O_APPEND | O_CREAT, 0666);
//     int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);
//     write(fd, mes, sizeof(mes));
//     close(fd);

//     return OCF_SUCCESS;
// }

// int
// metadata(int stdout, int stderr) {
//     static const char mes[] = "Hello from 'Meta-data action' test agent.\n";
//     int fd = open("/var/log/test.log", O_WRONLY | O_APPEND);

//     write(fd, mes, sizeof(mes));
//     close(fd);

//     static const char meta_data[]=
//     "<?xml version=\"1.0\"?>\n"
//     "<!DOCTYPE resource-agent SYSTEM \"ra-api-1.dtd\">\n"
//     "<resource-agent name=\"libtest.so\" version=\"1.0\">\n"
//     "  <version>1.0</version>\n"
//     "  <longdesc lang=\"en\">\n"
//     "   This script is used to write data to a file.\n"
//     "  </longdesc>\n"
//     "  <shortdesc lang=\"en\">Test agent</shortdesc>\n"
//     "  <actions>\n"
//     "    <action name=\"start\"   timeout=\"15s\" />\n"
//     "    <action name=\"stop\"    timeout=\"15s\" />\n"
//     "    <action name=\"monitor\" timeout=\"15s\" interval=\"15s\" />\n"
//     "    <action name=\"meta-data\"  timeout=\"5s\" />\n"
//     "  </actions>\n"
//     "</resource-agent>\n";

//     write(stdout, meta_data, sizeof(meta_data));
//     return OCF_SUCCESS;
// }


// int
// handler(int out_fd, int err_fd, char *action) {
//     int ret = 0;
//     if (strcmp(action, START_CMD) == 0){
//         ret = start();
//     } else if (strcmp(action, STOP_CMD) == 0) {
//         ret = stop();
//     } else if (strcmp(action, STATUS_CMD) == 0) {
//         ret = status();
//     } else if (strcmp(action, MONITOR_CMD) == 0) {
//         ret = monitor();
//     } else if (strcmp(action, META_DATA_CMD) == 0) {
//         ret = metadata(out_fd, err_fd);
//     }

//     return ret;
// }

// int
// mmain() {
//     // return code
//     int ret = 0;
//     // is used to get parameters
//     char *stack_address = __builtin_frame_address(0);
//     // skip 24 bytes: ret address, argc, exec name
//     char **argv = stack_address + 0x18;
//     // parse action
//     if (strcmp(*argv, START_CMD) == 0){
//         ret = start();
//     } else if (strcmp(*argv, STOP_CMD) == 0) {
//         ret = stop();
//     } else if (strcmp(*argv, STATUS_CMD) == 0) {
//         ret = status();
//     } else if (strcmp(*argv, MONITOR_CMD) == 0) {
//         ret = monitor();
//     } else if (strcmp(*argv, META_DATA_CMD) == 0) {
//         ret = metadata(1, 2);
//     }

//     _exit(ret);
// }