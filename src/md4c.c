/*
 * Build shim for mruby's non-recursive gem source discovery.
 *
 * The vendored md4c sources live under src/md4c/, but mruby only compiles
 * top-level src/* files by default.
 */
#include "md4c/md4c.c"
