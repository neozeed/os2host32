/* C/386 migration helper lowering. Original executable bytes on disk stay intact.
 * No real 16-bit execution or selector tokens are claimed by this bridge. */
struct Far16Desc { const char *module,*name; uint32_t ordinal; uint8_t count,width[7]; };
static const struct Far16Desc far16_apis[]={
 {"VIOCALLS","VioScrollUp",7,7,{2,4,2,2,2,2,2}},
 {"VIOCALLS","VioGetCurPos",9,3,{2,4,4}},
 {"VIOCALLS","VioSetCurPos",15,3,{2,2,2}},
 {"VIOCALLS","VioWrtTTY",19,3,{2,2,4}},
 {"VIOCALLS","VioGetMode",21,2,{2,4}},
 {"KBDCALLS","KbdCharIn",4,3,{2,2,4}},
 {"KBDCALLS","KbdFlushBuffer",13,1,{2}}
};
static int far16_desc(const char *module,uint32_t ordinal)
{
 uint32_t i;for(i=0;i<7;i++)if(!_stricmp(module,far16_apis[i].module)&&ordinal==far16_apis[i].ordinal)return (int)i;
 return -1;
}
static void install_far16(struct LeImage *x,struct Runtime *rt)
{
 uint32_t i,j,k,n=0,covered[MAX_OBJECTS]={0};
 struct {uint32_t obj,off,desc;} helpers[64];
 static const uint8_t pro[]={0x55,0x8b,0xec,0x83,0xec,4,0x53,0x57,0x56,6};
 static const uint8_t lss[]={0x66,0x0f,0xb2,0x24,0x24};
 for(i=0;i<x->nbridge_fix;i++) {
  struct BridgeFix *e=&x->bridge_fix[i],*ret=NULL,*alias=NULL,*stack=NULL;
  uint32_t base,sel,lo,helper=0xffffffffu;uint8_t *code,*thunk;int d;
  if(e->kind!=TGT_EXT_ORD || (e->type&0x1f)!=(SRC_PTR16|0x10))continue;
  if(e->used || !e->off || n==64)fatal("invalid migration import");
  d=far16_desc(x->modules[e->first-1].name,e->target);if(d<0)fatal("unknown far16 API");
  base=e->off-1;
  if((x->objects[e->obj].flags&OBJ_BIG)||x->objects[e->obj].size<14||base>x->objects[e->obj].size-14)fatal("invalid migration fragment");
  thunk=rt->ram+x->objects[e->obj].mapped_addr+base;
  if(thunk[0]!=0x9a||thunk[5]!=0x66||thunk[6]!=0x67||thunk[7]!=0xea)fatal("unrecognized far16 fragment");
  for(j=0;j<x->nbridge_fix;j++) {
   struct BridgeFix *b=&x->bridge_fix[j];
   if(!b->used&&b->obj==e->obj&&b->off==base+8&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==SRC_PTR32) {
    if(ret)fatal("ambiguous far16 return");ret=b;
   }
  }
  if(!ret)fatal("missing far16 return");
  for(j=0;j<x->nbridge_fix;j++) {
   struct BridgeFix *b=&x->bridge_fix[j];
   if(!b->used&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==(SRC_SEL16|0x10)&&b->first==e->obj+1&&b->obj==ret->first-1&&b->off+2==ret->target) {
    if(alias)fatal("ambiguous far16 alias");alias=b;
   }
  }
  if(!alias || !(x->objects[alias->obj].flags&OBJ_BIG))fatal("missing 32-bit migration helper");
  sel=alias->off;code=rt->ram+x->objects[alias->obj].mapped_addr;
  if(sel<9 || code[sel-4]!=0x66 || code[sel-3]!=0xea || rd16(code+sel-2)!=base || memcmp(code+sel-9,lss,5))fatal("invalid migration transition");
  lo=sel>192?sel-192:0;
  for(j=lo;j+sizeof(pro)<=sel;j++)if(!memcmp(code+j,pro,sizeof(pro)))helper=j;
  if(helper==0xffffffffu)fatal("missing migration prologue");
  for(j=0;j<x->nbridge_fix;j++) {
   struct BridgeFix *b=&x->bridge_fix[j];
   if(!b->used&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==SRC_SEL16&&b->obj==alias->obj&&b->first==x->stack_object&&b->off>helper+1&&b->off<sel) {
    if(stack)fatal("ambiguous migration stack");stack=b;
   }
  }
  if(!stack || code[stack->off-2]!=0x66 || code[stack->off-1]!=0x3d)fatal("missing migration SS comparison");
  for(k=0;k<n;k++)if(helpers[k].obj==alias->obj&&helpers[k].off==helper)fatal("overlapping migration helpers");
  helpers[n].obj=alias->obj;helpers[n].off=helper;helpers[n++].desc=(uint32_t)d;
  covered[e->obj]+=14;e->used=ret->used=alias->used=stack->used=1;
 }
 for(i=0;i<x->nbridge_fix;i++)if(!x->bridge_fix[i].used)fatal("nonflat relocation outside recognized migration helpers");
 for(i=0;i<x->num_objects;i++)if(!(x->objects[i].flags&OBJ_BIG)&&covered[i]!=x->objects[i].size)fatal("16-bit object is not entirely migration fragments");
 /* Validate everything before installing replacements. Packed Pascal frame:
    return DWORD, rightmost arg first, WORD scalars / DWORD flat pointer tokens. */
 for(i=0;i<n;i++) {
  uint32_t va=align_up(rt->stub_next,16),src=x->objects[helpers[i].obj].mapped_addr+helpers[i].off,bytes=0;
  uint8_t *p;const struct Far16Desc *d=&far16_apis[helpers[i].desc];
  if(va+16>GUEST_STUB_LIMIT)fatal("far16 veneer overflow");p=rt->ram+va;
  for(j=0;j<d->count;j++)bytes+=d->width[j];
  p[0]=0xb8;wr32(p+1,HC_CONSOLE|(helpers[i].desc+1));p[5]=0xe7;p[6]=0xf0;
  p[7]=0xc2;p[8]=(uint8_t)bytes;p[9]=(uint8_t)(bytes>>8);
  rt->ram[src]=0xe9;wr32(rt->ram+src+1,va-(src+5));rt->stub_next=va+16;
  fprintf(stderr,"v2: C/386 bridge %s.%u %s helper=%08X packed=%u -> %08X\n",d->module,d->ordinal,d->name,src,bytes,va);
 }
}
