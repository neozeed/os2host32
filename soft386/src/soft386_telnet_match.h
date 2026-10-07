/* Exact TELNETPM-1993 identity, shared algorithm with the native profile. */
static uint32_t tp_ror(uint32_t v, unsigned n) { return (v >> n) | (v << (32 - n)); }
static void tp_sha256(const uint8_t *data, uint32_t size, uint8_t digest[32])
{
    static const uint32_t k[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t h[8], w[64], a,b,c,d,e,f,g,j,t1,t2,block,blocks,i,n,pos;
    uint8_t buf[64];
    static const uint32_t initial[8] = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                  0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    memcpy(h, initial, sizeof(h));
    blocks = size / 64 + ((size % 64 < 56) ? 1 : 2);
    for (block = 0; block < blocks; ++block) {
        memset(buf, 0, sizeof(buf));
        pos = block * 64;
        if (pos < size) {
            n = size - pos; if (n > 64) n = 64;
            memcpy(buf, data + pos, n);
        }
        if (pos <= size && size - pos < 64) buf[size - pos] = 0x80;
        if (block == blocks - 1) {
            buf[59] = (uint8_t)(size >> 29);
            n = size << 3;
            for (i = 0; i < 4; ++i) buf[63-i] = (uint8_t)(n >> (8*i));
        }
        for (i = 0; i < 16; ++i)
            w[i] = ((uint32_t)buf[i*4]<<24) | ((uint32_t)buf[i*4+1]<<16) |
                   ((uint32_t)buf[i*4+2]<<8) | buf[i*4+3];
        for (i = 16; i < 64; ++i)
            w[i] = w[i-16] + (tp_ror(w[i-15],7)^tp_ror(w[i-15],18)^(w[i-15]>>3)) +
                   w[i-7] + (tp_ror(w[i-2],17)^tp_ror(w[i-2],19)^(w[i-2]>>10));
        a=h[0]; b=h[1]; c=h[2]; d=h[3]; e=h[4]; f=h[5]; g=h[6]; j=h[7];
        for (i=0;i<64;++i) {
            t1=j+(tp_ror(e,6)^tp_ror(e,11)^tp_ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            t2=(tp_ror(a,2)^tp_ror(a,13)^tp_ror(a,22))+((a&b)^(a&c)^(b&c));
            j=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=j;
    }
    for (i=0;i<32;++i) digest[i]=(uint8_t)(h[i/4]>>(24-8*(i%4)));
}

static int tp_match(struct LeImage *x)
{
    static const uint8_t expected[32] = {
        0x16,0xf3,0x4d,0x71,0x2c,0xde,0xfb,0x8b,0x3f,0x4f,0xa9,0x3b,0x79,0x56,0xfc,0x97,
        0x42,0x96,0x97,0x41,0x9d,0xec,0xcd,0x80,0xb6,0x2b,0x8d,0x83,0x8d,0x49,0x60,0xe2};
    uint8_t hash[32];
    if (x->file_size != 273544UL || !x->is_lx) return 0;
    tp_sha256(x->file, x->file_size, hash);
    return memcmp(hash, expected, 32) == 0;
}


static int tp_canonical_etc(char *env,uint32_t size)
{
    uint32_t pos,len;
    int changed;
    char *end;
    pos=0; changed=0;
    while(pos<size) {
        end=(char *)memchr(env+pos,0,size-pos);
        if(!end) return -1;
        len=(uint32_t)(end-(env+pos));
        if(!len) return changed;
        if(len>=4 && (env[pos]=='e' || env[pos]=='E') &&
           (env[pos+1]=='t' || env[pos+1]=='T') &&
           (env[pos+2]=='c' || env[pos+2]=='C') && env[pos+3]=='=') {
            if(memcmp(env+pos,"ETC",3)) { memcpy(env+pos,"ETC",3); ++changed; }
        }
        pos+=len+1;
    }
    return -1;
}

