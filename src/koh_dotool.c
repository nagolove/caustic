#include "koh_dotool.h"

void dotool_init(struct dotool_ctx *ctx) {
}

void dotool_shutdown(struct dotool_ctx *ctx) {
}

void dotool_send_signal(struct dotool_ctx *ctx) {
}

void dotool_exec_script(struct dotool_ctx *ctx) {
    if (!app_args.is_xdotool) 
        return;

    printf("dotool_exec_script: xdotool_fname %s\n", app_args.xdotool_fname);

    pid_t ret = fork();
    if (ret == -1) {
        printf("dotool_exec_script: fork() failed\n");
        exit(EXIT_FAILURE);
    }

    if (!ret) {
        // TODO: Ждать пока приложение не будет готово к взаимодействию с 
        // устройствами ввода

        sleep(2);
        printf("dotool_exec_script: before execve\n");
        execve(
            "/usr/bin/xdotool",
            (char *const []){ 
                (char*)app_args.xdotool_fname,
                NULL 
            },
            __environ
        );
    }
}
