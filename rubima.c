#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mruby.h"
#include "mruby/irep.h"
#include "mruby_gdb.h"

int main(void)
{
    static mrb_state *mrb = NULL;
    mrb_value ret;

    mrb = mrb_open();
    mrb->code_fetch_hook = mrb_gdb_code_fetch;

    #include "rubycode.h"
    ret = mrb_load_irep(mrb, code);

    mrb_close(mrb);

    return 0;
}
