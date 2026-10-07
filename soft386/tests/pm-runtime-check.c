/* Headless provider for the REAL Tiny386/LE loader/PM bridge. The fixture's
 * window procedure is LX code in PMJAR.DLL, never a native test callback. */
#define SOFT386_TEST_PROVIDER
#define main soft386_vessel_main
#include "../src/soft386_os2.c"
#undef main
#include <assert.h>
#ifndef __cdecl
#define __cdecl
#endif
static struct Soft386PmBridge *bridge;
static uint32_t (__cdecl *window_proc)(uint32_t,uint32_t,uint32_t,uint32_t);
static unsigned creates,paints,texts,destroys,shows,peeks;
#define HOST_WINDOW 0xe1234567u
#define HOST_PS 0xdf123456u
static void host_buffer(uintptr_t p,size_t n){struct Runtime *r=bridge->opaque;assert(p);assert(p+n<p||(p+n<=(uintptr_t)r->ram||p>=(uintptr_t)r->ram+RAM_SIZE));}
static uint32_t f_init(uintptr_t reserved){assert(!reserved);return 1;}
static uint32_t f_queue(uintptr_t hab,uintptr_t count){assert(hab==1&&!count);return 0xd1234567;}
static uint32_t f_register(uintptr_t hab,uintptr_t name,uintptr_t callback,uintptr_t style,uintptr_t extra){assert(hab==1&&style==4&&extra==4);host_buffer(name,11);assert(!strcmp((char *)name,"Soft386R5"));assert(callback&&callback>RAM_SIZE);window_proc=(void *)callback;return 1;}
static uint32_t f_create(uintptr_t parent,uintptr_t style,uintptr_t flags,uintptr_t cls,uintptr_t title,uintptr_t cstyle,uintptr_t module,uintptr_t resources,uintptr_t out){assert(parent==1&&style==0x80000000u&&!cstyle&&!resources&&module==0x1000);host_buffer(flags,4);host_buffer(cls,11);host_buffer(title,4);host_buffer(out,4);assert(*(uint32_t *)flags==0x1b);assert(window_proc);assert(!window_proc(HOST_WINDOW,1,0,0));*(uint32_t *)out=HOST_WINDOW;creates++;return HOST_WINDOW;}
static uint32_t f_show(uintptr_t hwnd,uintptr_t show){assert(hwnd==HOST_WINDOW&&show==1);shows++;return 1;}
static uint32_t f_peek(uintptr_t hab,uintptr_t msg,uintptr_t hwnd,uintptr_t first,uintptr_t last,uintptr_t flags){uint8_t *p=(void *)msg;assert(hab==1&&!hwnd&&!first&&!last&&(flags==0||flags==1));host_buffer(msg,28);peeks++;if(paints||!shows)return 0;memset(p,0,28);wr32(p,HOST_WINDOW);p[4]=0x0f;wr32(p+8,0xaabbccdd);wr32(p+12,0x99887766);wr32(p+16,12345);return 1;}
static uint32_t f_dispatch(uintptr_t hab,uintptr_t msg){uint8_t *p=(void *)msg;assert(hab==1);host_buffer(msg,28);assert(rd32(p)==HOST_WINDOW&&p[4]==0x0f);assert(rd32(p+8)==0xaabbccdd&&rd32(p+12)==0x99887766);paints++;return window_proc(HOST_WINDOW,0x23,0,0);}
static uint32_t f_paint(uintptr_t hwnd,uintptr_t ps,uintptr_t rect){assert(hwnd==HOST_WINDOW&&!ps);host_buffer(rect,16);wr32((uint8_t *)rect,0);wr32((uint8_t *)rect+4,0);wr32((uint8_t *)rect+8,320);wr32((uint8_t *)rect+12,200);return HOST_PS;}
static uint32_t f_fill(uintptr_t ps,uintptr_t rect,uintptr_t color){assert(ps==HOST_PS&&(uint32_t)color==0xfffffffe);host_buffer(rect,16);assert(rd32((uint8_t *)rect+8)==320);return 1;}
static uint32_t f_color(uintptr_t ps,uintptr_t color){assert(ps==HOST_PS&&color==2);return 0;}
static uint32_t f_text(uintptr_t ps,uintptr_t point,uintptr_t len,uintptr_t text){assert(ps==HOST_PS);host_buffer(point,8);host_buffer(text,len);assert(rd32((void *)point)==12);assert(!memcmp((void *)text,"Soft386 PM:",11));texts++;return 1;}
static uint32_t f_endpaint(uintptr_t ps){assert(ps==HOST_PS);return 1;}
static uint32_t f_destroy(uintptr_t hwnd){assert(hwnd==HOST_WINDOW);destroys++;window_proc(HOST_WINDOW,2,0,0);return 1;}
static uint32_t f_destroy_queue(uintptr_t hmq){assert(hmq==0xd1234567);return 1;}
static uint32_t f_term(uintptr_t hab){assert(hab==1);return 1;}
static uint32_t f_load_string(uintptr_t hab,uintptr_t module,uintptr_t id,uintptr_t cap,uintptr_t out){unsigned i;assert(hab==1&&module==0x1000&&!id&&cap==64);host_buffer(out,cap);for(i=0;i<bridge->nresources;i++){struct Soft386PmResource *r=&bridge->resources[i];if(r->module==module&&r->type==5&&r->id==1){const uint8_t *p=r->data;host_buffer((uintptr_t)p,r->size);assert(getenv("SOFT386_TEST_NO_RESOURCE_CHECK")==NULL);assert(p[2]==13&&!memcmp(p+3,"Jar resource\0",13));memcpy((void *)out,p+3,13);return 12;}}assert(!"resource missing");return 0;}
static void *get_pm(void *opaque,uint32_t ordinal){(void)opaque;switch(ordinal){
#define P(o,f) case o:return (void *)(uintptr_t)f
 P(763,f_init);P(716,f_queue);P(926,f_register);P(908,f_create);P(883,f_show);P(918,f_peek);P(912,f_dispatch);P(703,f_paint);P(743,f_fill);P(738,f_endpaint);P(728,f_destroy);P(726,f_destroy_queue);P(888,f_term);P(781,f_load_string);
 default:return NULL;}}
static void *get_gpi(void *opaque,uint32_t ordinal){(void)opaque;switch(ordinal){P(517,f_color);P(359,f_text);default:return NULL;}}
#undef P
static void check_term(void *opaque){struct Soft386PmBridge *b=opaque;struct Runtime *rt=b->opaque;struct GuestModule *g=module_handle(rt,0x1000);if(rt->process_rc==0){assert(g&&g->term_called);assert(guest_u32(rt,g->image.objects[1].mapped_addr+4)==2);}}
void soft386_test_pm_provider(struct Soft386PmBridge *b){bridge=b;soft386_pm_set_provider(b,0,NULL,get_pm);soft386_pm_set_provider(b,1,b,get_gpi);b->module[1].close=check_term;}
int main(int argc,char **argv){int rc=soft386_vessel_main(argc,argv);if(rc){fprintf(stderr,"PM runtime failed rc=%d creates=%u paints=%u texts=%u destroys=%u peeks=%u\n",rc,creates,paints,texts,destroys,peeks);return rc;}assert(creates==1&&paints==1&&texts==1&&destroys==1&&shows==1&&peeks>1);puts("Soft386 R5 headless PM runtime PASS: DLL callback, nesting, yielding, owner TID, FS/x87, resources, QMSG tokens");return 0;}
