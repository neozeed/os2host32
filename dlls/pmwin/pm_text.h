/* OS/2 RT_STRING and RT_MESSAGE share 16-entry bundles: codepage WORD,
 * then byte lengths including each string's NUL. Keep malformed resources
 * bounded by their registered size, and always reserve the output NUL. */
static LONG pm_text_copy(const unsigned char *data,DWORD size,unsigned index,
                         char *out,LONG capacity)
{
    DWORD pos=2,length,chars; unsigned i;
    if(!out || capacity<=0) return 0;
    out[0]=0;
    if(!data || size<3 || index>=16) return 0;
    for(i=0;i<=index;++i) {
        if(pos>=size) return 0;
        length=data[pos++];
        if(!length || length>size-pos || data[pos+length-1]!=0) return 0;
        if(i==index) {
            chars=length-1;
            if(chars>=(DWORD)capacity) chars=(DWORD)capacity-1;
            memcpy(out,data+pos,chars); out[chars]=0; return (LONG)chars;
        }
        pos+=length;
    }
    return 0;
}
