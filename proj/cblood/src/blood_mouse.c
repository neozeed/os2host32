#include "blood_mouse.h"
#include <string.h>
BloodFlightInput blood_mouse_flight(int x,int y,uint8_t buttons)
{
    BloodFlightInput in;
    memset(&in,0,sizeof(in));
    if(x<8)x=8;
    if(x>288)x=288;
    if(y<108)y=108;
    if(y>196)y=196;
    in.horizontal=(int16_t)(y-175);
    in.vertical=(int16_t)(x-160);
    in.mouse_mode=1;
    /* 4D75 left brake, then 4D84 right overwrites it: right wins together. */
    in.speed_event=(buttons&2u)?0x40u:(buttons&1u)?0x80u:0u;
    in.button_code=in.speed_event&0x80u;
    return in;
}
