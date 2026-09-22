/* R11: bounded, guest-handle-based bitmap subset. All rows are bottom-up.
   OS/2 headers/palettes are decoded explicitly; never cast guest structures
   to native BITMAPINFO or expose a host pointer as a guest handle. */
#define GPI_SLOTS 16
static void gpi_wr16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
struct GpiBitmap {
    uint32_t id,w,h,bpp,stride;
    uint8_t *bits;
    uint32_t rgb[256]; /* 00RRGGBB */
};
struct GpiPS {uint32_t id,dc,bitmap,color,rgb_mode;uint32_t logical[256];};
struct GuestGPI {
    uint32_t serial,bytes,dc[GPI_SLOTS];
    struct GpiPS ps[GPI_SLOTS];
    struct GpiBitmap bm[GPI_SLOTS];
};
struct GpiInfo {uint32_t cb,w,h,bpp,stride,colors,step;};
static void gpi_cleanup(struct Runtime *rt)
{
    unsigned i;
    if(!rt->gpi)return;
    for(i=0;i<GPI_SLOTS;i++)free(rt->gpi->bm[i].bits);
    free(rt->gpi);rt->gpi=NULL;
}
static struct GpiPS *gpi_ps(struct Runtime *rt,uint32_t id)
{
    unsigned i;
    if(!rt->gpi || !id)return NULL;
    for(i=0;i<GPI_SLOTS;i++)if(rt->gpi->ps[i].id==id)return &rt->gpi->ps[i];
    return NULL;
}
static int gpi_window_ps(struct Runtime *rt,uint32_t id)
{
    struct GpiPS *p=gpi_ps(rt,id);return p && p->dc==PM_WINDOW_DC;
}
static struct GpiBitmap *gpi_bm(struct Runtime *rt,uint32_t id)
{
    unsigned i;
    if(!rt->gpi || !id)return NULL;
    for(i=0;i<GPI_SLOTS;i++)if(rt->gpi->bm[i].id==id)return &rt->gpi->bm[i];
    return NULL;
}
static uint32_t *gpi_dc(struct Runtime *rt,uint32_t id)
{
    unsigned i;
    if(!rt->gpi || !id)return NULL;
    for(i=0;i<GPI_SLOTS;i++)if(rt->gpi->dc[i]==id)return &rt->gpi->dc[i];
    return NULL;
}
static uint32_t gpi_id(struct Runtime *rt)
{
    if(rt->gpi->serial>=0xffffffu)return 0;
    return 0x60000000u | ++rt->gpi->serial;
}
static int gpi_info(struct Runtime *rt,uint32_t addr,struct GpiInfo *f,int palette)
{
    uint32_t planes;
    if(!addr || !guest_range(addr,4))return 0;
    memset(f,0,sizeof(*f));f->cb=guest_u32(rt,addr);
    if((f->cb!=12 && f->cb!=16 && f->cb!=64) || !guest_range(addr,f->cb))return 0;
    if(f->cb==12) {
        f->w=rd16(rt->ram+addr+4);f->h=rd16(rt->ram+addr+6);
        planes=rd16(rt->ram+addr+8);f->bpp=rd16(rt->ram+addr+10);f->step=3;
    } else {
        f->w=guest_u32(rt,addr+4);f->h=guest_u32(rt,addr+8);
        planes=rd16(rt->ram+addr+12);f->bpp=rd16(rt->ram+addr+14);f->step=4;
        if(f->cb==64 && (guest_u32(rt,addr+16) || rd16(rt->ram+addr+44) || guest_u32(rt,addr+56)))return 0;
    }
    if(planes!=1 || !f->w || !f->h || f->w>2048 || f->h>2048 ||
       (f->bpp!=1 && f->bpp!=4 && f->bpp!=8 && f->bpp!=24 && f->bpp!=32))return 0;
    f->stride=((f->w*f->bpp+31u)/32u)*4u;
    f->colors=f->bpp<=8 ? 1u<<f->bpp : 0;
    if(f->cb==64 && guest_u32(rt,addr+32)) {
        if(guest_u32(rt,addr+32)>f->colors)return 0;
        f->colors=guest_u32(rt,addr+32);
    }
    return !palette || guest_range(addr,f->cb+f->colors*f->step);
}
static void gpi_palette(struct Runtime *rt,struct GpiBitmap *b,uint32_t addr,const struct GpiInfo *f)
{
    uint32_t i;
    for(i=0;i<f->colors;i++) {
        const uint8_t *p=rt->ram+addr+f->cb+i*f->step;
        b->rgb[i]=((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
    }
}
static uint32_t gpi_pixel(const struct GpiBitmap *b,uint32_t x,uint32_t y)
{
    const uint8_t *p=b->bits+y*b->stride;
    if(b->bpp==1)return b->rgb[(p[x/8]>>(7-x%8))&1];
    if(b->bpp==4)return b->rgb[(p[x/2]>>((x&1)?0:4))&15];
    if(b->bpp==8)return b->rgb[p[x]];
    p+=x*(b->bpp/8);return ((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
}
static void gpi_putpixel(struct GpiBitmap *b,uint32_t x,uint32_t y,uint32_t rgb)
{
    uint8_t *p=b->bits+y*b->stride;
    uint32_t i,best=0,dist=0xffffffffu;
    if(b->bpp<=8) {
        for(i=0;i<(1u<<b->bpp);i++) {
            int dr=(int)((rgb>>16)&255)-(int)((b->rgb[i]>>16)&255);
            int dg=(int)((rgb>>8)&255)-(int)((b->rgb[i]>>8)&255);
            int db=(int)(rgb&255)-(int)(b->rgb[i]&255);
            uint32_t d=(uint32_t)(dr*dr+dg*dg+db*db);
            if(d<dist){dist=d;best=i;if(!d)break;}
        }
        if(b->bpp==8)p[x]=(uint8_t)best;
        else if(b->bpp==4){unsigned sh=(x&1)?0:4;p[x/2]=(uint8_t)((p[x/2]&~(15u<<sh))|(best<<sh));}
        else {unsigned sh=7-x%8;p[x/8]=(uint8_t)((p[x/8]&~(1u<<sh))|(best<<sh));}
    } else {
        p+=x*(b->bpp/8);p[0]=(uint8_t)rgb;p[1]=(uint8_t)(rgb>>8);p[2]=(uint8_t)(rgb>>16);
        if(b->bpp==32)p[3]=0;
    }
}
static uint32_t gpi_bits(struct Runtime *rt,struct GpiBitmap *b,uint32_t start,uint32_t count,
                         uint32_t data,uint32_t info,int query)
{
    struct GpiInfo f;
    uint32_t i;
    if(!b || start>=b->h || (int32_t)count<0 || !gpi_info(rt,info,&f,1) ||
       f.w!=b->w || f.h!=b->h || f.bpp!=b->bpp)return 0;
    if(count>b->h-start)count=b->h-start;
    if(!data || !guest_range(data,count*b->stride))return 0;
    if(query) {
        memcpy(rt->ram+data,b->bits+start*b->stride,count*b->stride);
        for(i=0;i<f.colors;i++) {
            uint8_t *p=rt->ram+info+f.cb+i*f.step;
            p[0]=(uint8_t)b->rgb[i];p[1]=(uint8_t)(b->rgb[i]>>8);p[2]=(uint8_t)(b->rgb[i]>>16);
            if(f.step==4)p[3]=0;
        }
    } else {
        gpi_palette(rt,b,info,&f);
        memcpy(b->bits+start*b->stride,rt->ram+data,count*b->stride);
    }
    return count;
}
/* Point 1 is the exclusive upper-right boundary, as in the V1 bridge.
   First implementation: positive rectangles, SRCCOPY, nearest-neighbor scale. */
static uint32_t gpi_blt(struct Runtime *rt,const uint32_t *a)
{
    struct GpiPS *sp=gpi_ps(rt,a[1]),*dp=gpi_ps(rt,a[0]);
    struct GpiBitmap *src=sp?gpi_bm(rt,sp->bitmap):NULL,*dst=dp?gpi_bm(rt,dp->bitmap):NULL;
    int32_t pt[8];uint32_t i,x,y,sw,sh,dw,dh,*pixels;RECT cr;
    int screen=(a[0]==PM_HPS && rt->pm->paint_dc) || (gpi_window_ps(rt,a[0]) && rt->pm->window);
    if(!src || (!dst && !screen) || (a[2]!=3 && a[2]!=4) || a[4]!=0xcc || a[5]>2 ||
       !a[3] || !guest_range(a[3],a[2]*8))return 0;
    for(i=0;i<a[2]*2;i++)pt[i]=(int32_t)guest_u32(rt,a[3]+i*4);
    for(i=0;i<a[2]*2;i++)if(pt[i]<0 || pt[i]>16384)return 0;
    if(pt[2]<=pt[0] || pt[3]<=pt[1])return 0;
    dw=(uint32_t)(pt[2]-pt[0]);dh=(uint32_t)(pt[3]-pt[1]);
    if(a[2]==3){sw=dw;sh=dh;}
    else {if(pt[6]<=pt[4] || pt[7]<=pt[5])return 0;sw=(uint32_t)(pt[6]-pt[4]);sh=(uint32_t)(pt[7]-pt[5]);}
    if((uint32_t)pt[4]+sw>src->w || (uint32_t)pt[5]+sh>src->h)return 0;
    if(dst && ((uint32_t)pt[2]>dst->w || (uint32_t)pt[3]>dst->h))return 0;
    pixels=(uint32_t *)malloc(sw*sh*4u);if(!pixels)return 0;
    /* Snapshot handles overlapping copies, and supplies a top-down native DIB. */
    for(y=0;y<sh;y++)for(x=0;x<sw;x++)
        pixels[(sh-1-y)*sw+x]=gpi_pixel(src,(uint32_t)pt[4]+x,(uint32_t)pt[5]+y);
    if(screen) {
        BITMAPINFO bi;int rc;HDC dc;int release;
        if(!GetClientRect(rt->pm->window,&cr)){free(pixels);return 0;}
        memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth=(LONG)sw;bi.bmiHeader.biHeight=-(LONG)sh;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        release=!(rt->pm->paint_dc && rt->pm->paint_hps==a[0]);
        dc=release?GetDC(rt->pm->window):rt->pm->paint_dc;
        if(!dc){free(pixels);return 0;}
        SetStretchBltMode(dc,COLORONCOLOR);
        rc=StretchDIBits(dc,pt[0],cr.bottom-pt[3],(int)dw,(int)dh,
                       0,0,(int)sw,(int)sh,pixels,&bi,DIB_RGB_COLORS,SRCCOPY);
        if(release)ReleaseDC(rt->pm->window,dc);
        free(pixels);return rc!=0 && (uint32_t)rc!=0xffffffffu;
    }
    for(y=0;y<dh;y++)for(x=0;x<dw;x++)
        gpi_putpixel(dst,(uint32_t)pt[0]+x,(uint32_t)pt[1]+y,pixels[(sh-1-y*sh/dh)*sw+x*sw/dw]);
    free(pixels);return 1;
}
static uint32_t dispatch_gpi(struct Runtime *rt,uint32_t ordinal,uint32_t esp)
{
    uint32_t a[6],i,j,n=6,old;struct GpiPS *p;struct GpiBitmap *b;struct GpiInfo f;
    struct GuestThread *t=current_guest_thread(rt);
    if(!rt->pm || !t)return 0; /* shared PSs; guest mutexes serialize callers */
    if(ordinal==379 || ordinal==371 || ordinal==604)n=1;
    else if(ordinal==506 || ordinal==517 || ordinal==505 || ordinal==544 || ordinal==601)n=2;
    else if(ordinal==369)n=4;
    else if(ordinal==598 || ordinal==599 || ordinal==602)n=5;
    if(!guest_range(esp,(n+1)*4))return 0;
    memset(a,0,sizeof(a));for(i=0;i<n;i++)a[i]=guest_u32(rt,esp+4+i*4);
    fprintf(stderr,"v2: PMGPI.%u(%08X,%08X,%08X,%08X)\n",ordinal,a[0],a[1],a[2],a[3]);
    if(!rt->gpi){rt->gpi=(struct GuestGPI *)calloc(1,sizeof(*rt->gpi));if(!rt->gpi)return 0;}
    p=gpi_ps(rt,a[0]);b=p?gpi_bm(rt,p->bitmap):NULL;
    switch(ordinal) {
    case 610: /* memory DC only; token and driver data validated but not interpreted */
        if(a[0]!=PM_HAB || a[1]!=8 || a[3]>9 || (a[3] && (!a[4] || !guest_range(a[4],a[3]*4))) ||
           (a[5] && !gpi_dc(rt,a[5])))return 0;
        if(a[2]){char token[64];if(!guest_copy_cstr(rt,a[2],token,sizeof(token)))return 0;}
        for(i=0;i<GPI_SLOTS;i++)if(!rt->gpi->dc[i])return rt->gpi->dc[i]=gpi_id(rt);
        return 0;
    case 604:
    {
        uint32_t *dc=gpi_dc(rt,a[0]);if(!dc)return 0xffffffffu;
        for(i=0;i<GPI_SLOTS;i++)if(rt->gpi->ps[i].id && rt->gpi->ps[i].dc==a[0])return 0xffffffffu;
        *dc=0;return 0; /* memory DC has no metafile */
    }
    case 369:
        if(a[0]!=PM_HAB || (!gpi_dc(rt,a[1]) && !(a[1]==PM_WINDOW_DC && rt->pm->window)) || !a[2] || !guest_range(a[2],8) ||
           (a[3]&0xff)!=8 || (a[3]&~0x5208u))return 0;
        for(i=0;i<GPI_SLOTS;i++)if(!rt->gpi->ps[i].id) {
            p=&rt->gpi->ps[i];memset(p,0,sizeof(*p));p->id=gpi_id(rt);p->dc=a[1];p->color=0xffffffffu;
            for(j=0;j<256;j++){COLORREF c=pm_color(j%16);p->logical[j]=((c&255)<<16)|(c&0xff00)|((c>>16)&255);}
            return p->id;
        }
        return 0;
    case 379: if(!p)return 0;memset(p,0,sizeof(*p));return 1;
    case 598:
        if(!p || (a[2]&~4u) || !gpi_info(rt,a[1],&f,0))return 0;
        if(f.stride*f.h>32u*1024u*1024u-rt->gpi->bytes)return 0;
        for(i=0;i<GPI_SLOTS;i++)if(!rt->gpi->bm[i].id)break;
        if(i==GPI_SLOTS)return 0;
        b=&rt->gpi->bm[i];memset(b,0,sizeof(*b));
        b->w=f.w;b->h=f.h;b->bpp=f.bpp;b->stride=f.stride;
        b->bits=(uint8_t *)calloc(f.h,f.stride);if(!b->bits)return 0;
        for(j=0;j<256;j++)b->rgb[j]=p->logical[j];
        if((a[2]&4u) && gpi_bits(rt,b,0,b->h,a[3],a[4],0)!=b->h){free(b->bits);memset(b,0,sizeof(*b));return 0;}
        b->id=gpi_id(rt);rt->gpi->bytes+=b->stride*b->h;return b->id;
    case 506:
        if(!p || (a[1] && !gpi_bm(rt,a[1])))return 0xffffffffu;
        for(i=0;i<GPI_SLOTS;i++)if(a[1] && rt->gpi->ps[i].id && &rt->gpi->ps[i]!=p && rt->gpi->ps[i].bitmap==a[1])return 0xffffffffu;
        old=p->bitmap;p->bitmap=a[1];return old;
    case 371:
        b=gpi_bm(rt,a[0]);if(!b)return 0;
        for(i=0;i<GPI_SLOTS;i++)if(rt->gpi->ps[i].id && rt->gpi->ps[i].bitmap==a[0])return 0;
        rt->gpi->bytes-=b->stride*b->h;free(b->bits);memset(b,0,sizeof(*b));return 1;
    case 602: case 599:return gpi_bits(rt,b,a[1],a[2],a[3],a[4],ordinal==599);
    case 601:
        b=gpi_bm(rt,a[0]);if(!b || !a[1] || !guest_range(a[1],4))return 0;
        old=guest_u32(rt,a[1]);if((old!=12 && old!=16 && old!=64) || !guest_range(a[1],old))return 0;
        memset(rt->ram+a[1],0,old);guest_put_u32(rt,a[1],old);
        if(old==12){gpi_wr16(rt->ram+a[1]+4,(uint16_t)b->w);gpi_wr16(rt->ram+a[1]+6,(uint16_t)b->h);gpi_wr16(rt->ram+a[1]+8,1);gpi_wr16(rt->ram+a[1]+10,(uint16_t)b->bpp);}
        else {guest_put_u32(rt,a[1]+4,b->w);guest_put_u32(rt,a[1]+8,b->h);gpi_wr16(rt->ram+a[1]+12,1);gpi_wr16(rt->ram+a[1]+14,(uint16_t)b->bpp);if(old==64)guest_put_u32(rt,a[1]+20,b->stride*b->h);}
        return 1;
    case 592:
        if(!p || (a[1]&~7u))return 0;
        if(a[2]==2) { /* LCOLF_CONSECRGB */
            if(a[3]>256 || a[4]>256-a[3] || (a[4] && (!a[5] || !guest_range(a[5],a[4]*4))))return 0;
            for(i=0;i<a[4];i++)p->logical[a[3]+i]=guest_u32(rt,a[5]+i*4)&0xffffffu;
            if(b && b->bpp<=8)for(i=0;i<(1u<<b->bpp);i++)b->rgb[i]=p->logical[i];
            p->rgb_mode=0;return 1;
        }
        if((a[2]!=0 && a[2]!=3) || a[3] || a[4] || a[5])return 0;
        p->rgb_mode=a[2]==3;return 1;
    case 517: if(!p)return 0;p->color=a[1];return 1;
    case 505: return p && (a[1]==2 || a[1]==5);
    case 544:
        if(!p || !b || !a[1] || !guest_range(a[1],8))return 0;
        i=guest_u32(rt,a[1]);j=guest_u32(rt,a[1]+4);if(i>=b->w || j>=b->h)return 0;
        old=p->color;
        if(!p->rgb_mode){if(old<256)old=p->logical[old];else{COLORREF c=pm_color(old);old=((c&255)<<16)|(c&0xff00)|((c>>16)&255);}}
        gpi_putpixel(b,i,j,old&0xffffffu);return 1;
    case 355:return gpi_blt(rt,a);
    default:fprintf(stderr,"v2: unsupported PMGPI.%u\n",ordinal);return 0;
    }
}
