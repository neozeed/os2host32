/* Tests the emitted guest dispatcher, using a controlled callback body in RAM.
 * The supplied executable stays byte-for-byte untouched on disk. */
#define main soft386_vessel_main
#include "../src/soft386_os2.c"
#undef main
#include <assert.h>
int main(int argc,char **argv){
 struct Runtime *r=calloc(1,sizeof(*r));struct LeImage *x=calloc(1,sizeof(*x));CPUI386_State before,after;uint32_t args[6],result,base;
 static const uint8_t body[]={0x8b,0x44,0x24,4,0x03,0x44,0x24,8,0x03,0x44,0x24,12,0x03,0x44,0x24,16,0xc3};
 assert(argc==2&&r&&x);r->ram=calloc(1,RAM_SIZE);r->alloc_next=GUEST_ALLOC_BASE;r->module_next=GUEST_MODULE_BASE;r->stub_next=GUEST_STUB_BASE;r->main_image=x;r->current_thread=0;r->max_cycles=100000;
 x->file=load_file(argv[1],&x->file_size);assert(x->file);parse_header(x);parse_objects(x,r,1);assert(x->telnet_profile);parse_import_modules(x);scan_fixups(x);resolve_imports(r,x);apply_fixups(x,r->ram);emit_runtime_stubs(r);install_c386_helpers(x,r);
 base=x->objects[0].mapped_addr;memcpy(r->ram+base+0xb4d4,body,sizeof(body));
 r->cpu=cpui386_new(3,(char *)r->ram,RAM_SIZE,&r->cb);assert(r->cpu);cpui386_enable_fpu(r->cpu);r->cb->io=r;r->cb->io_write32=io_write32;
 cpui386_reset_pm_flat(r->cpu,base);cpui386_get_state(r->cpu,&before);init_gdt(r);
 before.gdt_base=GUEST_GDT;before.gdt_limit=GUEST_GDT_BYTES-1;before.gpr[4]=x->objects[4].mapped_addr+x->stack_offset-64;before.gpr[3]=0x89abcdef;before.flags=0x402;
 assert(cpui386_set_state(r->cpu,&before));r->threads[0].tid=1;r->threads[0].state=THREAD_RUNNING;r->threads[0].stack_base=x->objects[4].mapped_addr;r->threads[0].stack_size=x->stack_offset;
 cpui386_get_state(r->cpu,&r->threads[0].regs);init_thread_info(r,0);r->threads[0].regs_valid=1;assert(cpui386_set_state(r->cpu,&r->threads[0].regs));cpui386_get_state(r->cpu,&before);
 args[0]=1;args[1]=base+TP_CALLBACK;args[2]=12;args[3]=0x12345678;args[4]=0xaabb8123;args[5]=0xfedc9988;
 assert(invoke_guest(r,1,base+TP_DISPATCH,args,6,&result));assert(result==((0x12345678u+0x8123u+0x9988aabbu+0xfedcu)&65535));cpui386_get_state(r->cpu,&after);
 assert(!memcmp(before.gpr,after.gpr,sizeof(before.gpr))&&before.flags==after.flags&&before.seg[CPUI386_SEG_FS]==after.seg[CPUI386_SEG_FS]);
 args[0]=2;assert(!invoke_guest(r,1,base+TP_DISPATCH,args,6,&result)&&r->process_exited&&r->process_rc==1);
 cpui386_delete(r->cpu);free_le_image(x);free(x);free(r->ram);free(r);
 puts("R5C TELNETPM dispatcher CPU PASS: packed 4/2/4/2 arguments, flat guest callback, 16-bit return, saved registers/FS/DF, rejected external callback");return 0;
}
