static uint32_t gcall(uint32_t n,const uint32_t *a,size_t count)
{
 size_t i;for(i=0;i<count;i++)guest_put_u32(&rt,0x804+(uint32_t)i*4,a[i]);
 return dispatch_gpi(&rt,n,0x800);
}
#define GCALL(n,...) gcall(n,(const uint32_t[]){__VA_ARGS__},sizeof((const uint32_t[]){__VA_ARGS__})/4)
static void info64(uint32_t addr,uint32_t w,uint32_t h,uint16_t bits)
{
 memset(rt.ram+addr,0,64+1024);guest_put_u32(&rt,addr,64);
 guest_put_u32(&rt,addr+4,w);guest_put_u32(&rt,addr+8,h);
 gpi_wr16(rt.ram+addr+12,1);gpi_wr16(rt.ram+addr+14,bits);
}
static void test_gpi(void)
{
 uint32_t dc,ps,ps2,bm,bm2,tmp,i,n,formats[5]={1,4,8,24,32};
 struct GpiBitmap *b;
 assert(PCALL(763,0)==PM_HAB);
 assert(GCALL(610,PM_HAB,5,0,0,0,0)==0);
 dc=GCALL(610,PM_HAB,8,0,0,0,0);assert(dc);
 guest_put_u32(&rt,0x1000,3);guest_put_u32(&rt,0x1004,2);
 ps=GCALL(369,PM_HAB,dc,0x1000,0x5208);assert(ps);
 ps2=GCALL(369,PM_HAB,dc,0x1000,0x5208);assert(ps2 && ps2!=ps);
 assert(GCALL(604,dc)==0xffffffffu);
 /* All supported depths: odd widths, padded rows, exact partial readback. */
 for(n=0;n<5;n++) {
   info64(0x2000,3,2,(uint16_t)formats[n]);
   for(i=0;i<32;i++)rt.ram[0x3000+i]=(uint8_t)(i*17+3);
   bm=GCALL(598,ps,0x2000,4,0x3000,0x2000);assert(bm);
   assert(GCALL(506,ps,bm)==0);
   b=gpi_bm(&rt,bm);assert(b && b->stride%4==0);
   memset(rt.ram+0x4000,0xcc,64);
   assert(GCALL(599,ps,0,2,0x4000,0x2000)==2);
   assert(!memcmp(rt.ram+0x3000,rt.ram+0x4000,b->stride*2));
   assert(rt.ram[0x4000+b->stride*2]==0xcc);
   memset(rt.ram+0x3000,0x55,b->stride);
   assert(GCALL(602,ps,1,99,0x3000,0x2000)==1);
   assert(GCALL(599,ps,1,1,0x4000,0x2000)==1);
   assert(!memcmp(rt.ram+0x3000,rt.ram+0x4000,b->stride));
   assert(GCALL(602,ps,0,0xffffffffu,0x3000,0x2000)==0);
   assert(GCALL(602,ps,0,2,RAM_SIZE-1,0x2000)==0);
   assert(GCALL(506,ps,0)==bm && GCALL(371,bm)==1);
   assert(GCALL(506,ps,bm)==0xffffffffu); /* stale */
 }
 /* Legacy 12-byte header + packed three-byte palette and odd pixel nibble. */
 memset(rt.ram+0x2000,0,128);guest_put_u32(&rt,0x2000,12);
 gpi_wr16(rt.ram+0x2004,3);gpi_wr16(rt.ram+0x2006,2);
 gpi_wr16(rt.ram+0x2008,1);gpi_wr16(rt.ram+0x200a,4);
 rt.ram[0x2000+12+3]=255; /* index 1 blue */
 rt.ram[0x2000+12+6+2]=255; /* index 2 red */
 memset(rt.ram+0x3000,0,8);rt.ram[0x3000]=0x11;rt.ram[0x3001]=0x10;
 rt.ram[0x3004]=0x22;rt.ram[0x3005]=0x20;
 bm=GCALL(598,ps,0x2000,4,0x3000,0x2000);assert(bm);
 assert(GCALL(506,ps,bm)==0);
 assert(GCALL(506,ps2,bm)==0xffffffffu && !gpi_ps(&rt,ps2)->bitmap);
 assert(GCALL(371,bm)==0);
 b=gpi_bm(&rt,bm);assert(gpi_pixel(b,0,0)==0xff && gpi_pixel(b,0,1)==0xff0000);
 /* Palette roundtrip overwrites the supplied table with bitmap colors. */
 memset(rt.ram+0x200c,0xcc,48);
 assert(GCALL(599,ps,0,2,0x4000,0x2000)==2);
 assert(rt.ram[0x200f]==255 && rt.ram[0x2014]==255);
 assert(GCALL(592,ps,1,3,0,0,0)==1 && GCALL(517,ps,0xff0000)==1);
 guest_put_u32(&rt,0x1100,2);guest_put_u32(&rt,0x1104,0);
 assert(GCALL(544,ps,0x1100)==1 && gpi_pixel(b,2,0)==0xff0000);
 assert((b->bits[1]&15)==0); /* unrelated packed low nibble retained */
 assert(GCALL(505,ps,5)==1 && GCALL(505,ps,99)==0);
 /* Query both header generations without overrunning caller's buffer. */
 memset(rt.ram+0x5000,0xcc,68);guest_put_u32(&rt,0x5000,64);
 assert(GCALL(601,bm,0x5000)==1 && guest_u32(&rt,0x5004)==3 && guest_u32(&rt,0x5040)==0xcccccccc);
 guest_put_u32(&rt,0x5000,12);assert(GCALL(601,bm,0x5000)==1 && rd16(rt.ram+0x5004)==3);
 /* Copy through a distinct bitmap then render top-down DIB into PM paint DC. */
 bm2=GCALL(598,ps2,0x2000,4,0x4000,0x2000);assert(bm2);
 assert(GCALL(506,ps2,bm2)==0);
 for(i=0;i<8;i++)guest_put_u32(&rt,0x6000+i*4,(i==2||i==6)?3:(i==3||i==7)?2:0);
 assert(GCALL(355,ps2,ps,4,0x6000,0xcc,2)==1);
 assert(gpi_pixel(gpi_bm(&rt,bm2),2,0)==0xff0000);
 assert(GCALL(355,ps2,ps,4,0x6000,0x66,2)==0);
 assert(GCALL(355,ps2,ps,4,RAM_SIZE-8,0xcc,2)==0);
 assert(PCALL(716,PM_HAB,0)==PM_HMQ);strcpy((char *)rt.ram+0x900,"GPI");strcpy((char *)rt.ram+0xa00,"GPI test");
 assert(PCALL(926,PM_HAB,0x900,0x12000,4,0)==1);guest_put_u32(&rt,0xb00,0xc3b);cpu[1].Reg64=0x27000;
 assert(PCALL(908,1,0x80000000,0xb00,0x900,0xa00,0,0,0,0xb04)==0 && pm_entered);pm_return(0);
 assert(PCALL(703,PM_CLIENT,0,0)==PM_HPS);
 guest_put_u32(&rt,0x6000,10);guest_put_u32(&rt,0x6004,20);
 guest_put_u32(&rt,0x6008,310);guest_put_u32(&rt,0x600c,220);
 assert(GCALL(355,PM_HPS,ps,4,0x6000,0xcc,2)==1);
 assert(mock_dib_top==0xff0000 && mock_dib_bottom==0xff);
 assert(mock_dib_x==10 && mock_dib_y==140 && mock_dib_w==300 && mock_dib_h==200);
 assert(PCALL(738,PM_HPS)==1);
 assert(GCALL(355,PM_HPS,ps,4,0x6000,0xcc,2)==0);
 /* Invalid dimensions/compression/palette sizes do not allocate. */
 info64(0x2000,0xffffffffu,2,4);assert(!GCALL(598,ps,0x2000,0,0,0));
 info64(0x2000,3,2,4);guest_put_u32(&rt,0x2010,1);assert(!GCALL(598,ps,0x2000,0,0,0));
 guest_put_u32(&rt,0x2010,0);guest_put_u32(&rt,0x2020,17);assert(!GCALL(598,ps,0x2000,0,0,0));
 /* Recycled slots receive new handles; table exhaustion has no side effects. */
 assert(GCALL(506,ps,0)==bm && GCALL(371,bm)==1);
 info64(0x2000,3,2,4);tmp=GCALL(598,ps,0x2000,0,0,0);assert(tmp && tmp!=bm);
 assert(GCALL(506,ps,bm)==0xffffffffu);
 for(i=0;i<14;i++)assert(GCALL(598,ps,0x2000,0,0,0));
 assert(GCALL(598,ps,0x2000,0,0,0)==0);
 assert(GCALL(506,ps2,0)==bm2 && GCALL(371,bm2)==1);
 assert(GCALL(379,ps)==1 && GCALL(379,ps2)==1 && GCALL(604,dc)==0);
 assert(PCALL(728,PM_FRAME)==0 && pm_entered);pm_return(0);
 assert(PCALL(726,PM_HMQ)==1 && PCALL(888,PM_HAB)==1);
 assert(!rt.gpi && !rt.pm); /* frees intentionally abandoned unselected bitmaps */
 puts("gpi-check PASS: 1/4/8/24/32 bpp, headers, palette/row roundtrip, pixels, blits, bounds, handles, cleanup");
}
