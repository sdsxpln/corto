/* This is a managed file. Do not delete this comment. */

#include <loadSelf/loadSelf.h>


/* Enter code outside of main here. */

int cortomain(int argc, char *argv[]) {

    int ret = corto_use("loadSelf", 0, NULL);;
    return ret;
}
