/* This is a managed file. Do not delete this comment. */

#include <locateSelf/locateSelf.h>

/* Enter code outside of main here. */

int cortomain(int argc, char *argv[]) {

    const char *str = corto_locate("locateSelf", NULL, CORTO_LOCATE_LIB);
    if (!str) {
        goto error;
    }

    return 0;
error:
    return -1;
}
