/* host imports / exports for the wasm build */
#include "arm.h"
__attribute__((import_module("env"), import_name("js_log"))) void js_log(const char *s, int len);
void host_log(const char *s) { js_log(s, str_len(s)); }
