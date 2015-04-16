#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mruby.h"
#include "mruby/irep.h"
#include "mruby/compile.h"
#include "mruby/debug.h"
#include "mruby/opcode.h"
#include "mruby/value.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/proc.h"

#define RETURN_BUF_SIZE 4096
#define BUF_SIZE 256

const char *prev_filename = NULL;
const char * filename = NULL;
char ret[RETURN_BUF_SIZE];
int32_t prev_line = -1;
int32_t prev_ciidx = 999;
int32_t line = -1;
int32_t prev_callinfo_size = 0;

static void code_fetch(mrb_state* mrb, mrb_irep *irep, mrb_code *pc, mrb_value *regs);

//local変数の数を返す関数
static int
local_size(mrb_irep *irep){
    return irep->nlocals;
}

volatile int
md_strcmp(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

// mrb_get_callinfo()の結果の長さをとってCの整数(mrb_int)として返す関数
//ここの数（深さ）によってメソッド呼び出しが行われたかどうかがわかる
//（メソッド呼び出しがあると+1され、メソッドから戻ると-1される）
static mrb_int
mrb_gdb_get_callinfosize(mrb_state *mrb)
{
    mrb_int len = 0;
    mrb_callinfo *ci;
    mrb_int ciidx;
    int i;
    int lineno = -1;
    
    ciidx = mrb->c->ci - mrb->c->cibase;
    
    for (i = ciidx; i >= 0; i--) {
        ci = &mrb->c->cibase[i];
        if (MRB_PROC_CFUNC_P(ci->proc)) {
            continue;
        }else {
            mrb_irep *irep = ci->proc->body.irep;
            mrb_code *pc;
            
            if (mrb->c->cibase[i].err) {
                pc = mrb->c->cibase[i].err;
            }else if (i+1 <= ciidx) {
                pc = mrb->c->cibase[i+1].pc - 1;
            }else {
                pc = ci->pc; //continue;
            }
            lineno = mrb_debug_get_line(irep, pc - irep->iseq);
        }
        if (lineno == -1){
            continue;
        }
        len++;
    }
    return len;
}

static void
mrb_gdb_code_fetch(mrb_state* mrb, mrb_irep *irep, mrb_code *pc, mrb_value *regs)
{
    filename = mrb_debug_get_filename(irep, pc - irep->iseq);
    line = mrb_debug_get_line(irep, pc - irep->iseq);
    if(filename==NULL || line== -1){
        return;
    }
    if (prev_filename && filename && strcmp(prev_filename, filename) == 0 && prev_line == line) {
        return;
    }
    if (filename && line >= 0) {	// ここの行番号をブレークポイントに指定する
        int len = (int)mrb_gdb_get_callinfosize(mrb);
        if(prev_ciidx == 999){
            //一番最初のときは無条件にccidxを保存
            prev_filename = filename;
            prev_line = line;
            prev_ciidx = len;
            return;
        }
        prev_filename = filename;
        prev_line = line;
        prev_ciidx = len;
    }
}

static char *
mrb_gdb_get_current(mrb_state *mrb)
{
    int32_t  ciidx;
    memset(ret, 0, sizeof(ret));
    
    ciidx = (int32_t)mrb_gdb_get_callinfosize(mrb);
    snprintf(ret, sizeof(ret), "result={name=\"%s\",line=\"%d\",ciidx=\"%d\"}", filename, line, ciidx);
    return ret;
}

static char *
mrb_gdb_get_locals(mrb_state* mrb){
    const char *symname;
    char buf[BUF_SIZE];
    int i = 0;
    int flg = 0;	//
    mrb_irep *irep;
    int local_len = 0;
    
    if(mrb == NULL){
        return "mrb_null";
    }
    memset(ret, 0, sizeof(ret));
    irep = mrb->c->ci->proc->body.irep;
    
    local_len = local_size(irep);
    strncpy(ret, "locals=[", sizeof(ret)-1);
    mrb->code_fetch_hook = NULL;
    
    for (i=0; i+1<local_len; i++) {
        if (irep->lv[i].name == 0){
            continue;
        }
        uint16_t reg = irep->lv[i].r;
        mrb_sym sym = irep->lv[i].name;if (!sym){ continue;}
        symname = mrb_sym2name(mrb, sym);
        mrb_value v2 = mrb->c->stack[reg];
        const char *v2_classname = mrb_obj_classname(mrb, v2);
        mrb_value v2_value = mrb_funcall(mrb, v2, "inspect", 0);
        char * v2_cstr = mrb_str_to_cstr(mrb, v2_value);
        snprintf(buf, sizeof(buf), "{name=\"%s\",value=\"%s\",type=\"%s\"}", symname, v2_cstr, v2_classname);
        if(flg == 1){
            strncat(ret, ",", sizeof(ret)-strlen(ret)-1);
        }
        strncat(ret, buf, sizeof(ret)-strlen(ret)-1);
        flg = 1;
    }
    mrb->code_fetch_hook = mrb_gdb_code_fetch;
    return ret;
}

static char *
mrb_gdb_get_localvalue(mrb_state* mrb, char *symname){
    char buf[BUF_SIZE];
    int i = 0;
    mrb_irep *irep;
    int local_len = 0;
    
    if(mrb == NULL){
        return "mrb_null";
    }
    if(symname == NULL){
        return "sym_null";
    }
    memset(ret, 0, sizeof(ret));
    mrb_sym sym2 = mrb_intern_cstr(mrb, symname);
    if(sym2 == 0){
        return "bad_sym_null";  // _null でエラー処理を作ってしまったので変かもだけど気にするなw
    }
    
    irep = mrb->c->ci->proc->body.irep;
    if(irep != NULL){
        local_len = local_size(irep);
    }else{
        return "irep_null";
    }
    strncpy(ret, "result=", sizeof(ret)-1);
    for (i=0; i<local_len; i++) {
        mrb_sym sym = irep->lv[i].name;
        if(sym == sym2){
            uint16_t reg = irep->lv[i].r;
            mrb_value v2 = mrb->c->stack[reg];
            const char *v2_classname = mrb_obj_classname(mrb, v2);
            mrb->code_fetch_hook = NULL;
            mrb_value v2_value = mrb_funcall(mrb, v2, "inspect", 0);
            mrb->code_fetch_hook = mrb_gdb_code_fetch;
            char * v2_cstr = mrb_str_to_cstr(mrb, v2_value);
            snprintf(buf, sizeof(buf), "{name=\"%s\",value=\"%s\",type=\"%s\"}", symname, v2_cstr, v2_classname);
            strncat(ret, buf, sizeof(ret)-strlen(ret)-1);
        }
    }
    return ret;
}


int main(void)
{
	static mrb_state *mrb = NULL;
	mrb_value ret;

    int line = 0, cnt;
    for (line = 0; line < 10; line++) {
        cnt = line * 5;
        printf("result=%d",cnt);
    }
    
    mrb = mrb_open();
	mrb->code_fetch_hook = mrb_gdb_code_fetch;

	#include "rubycode.h"
	ret = mrb_load_irep(mrb, code);

	mrb_close(mrb);

	return 0;
}
