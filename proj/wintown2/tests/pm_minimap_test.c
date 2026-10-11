#include <assert.h>
#include <stdio.h>
#include "../os2pm/pm_minimap.h"
int main(void)
{
    int x, last;
    assert(pm_mini_position(4,4,360,1920,532)==0);
    assert(pm_mini_position(364,4,360,1920,532)==1388);
    assert(pm_mini_position(184,4,360,1920,532)==694);
    assert(pm_mini_position(0,0,300,1600,278)==0);
    assert(pm_mini_position(300,0,300,1600,278)==1322);
    assert(pm_mini_position(150,0,300,1600,278)==661);
    assert(pm_mini_position(-100,4,360,1920,532)==0);
    assert(pm_mini_position(999,4,360,1920,532)==1388);
    assert(pm_mini_position(200,4,360,1920,3000)==0);
    assert(pm_mini_position(200,4,0,1920,532)==0);
    last=0;
    for(x=0;x<=360;++x) {
        int next;
        next=pm_mini_position(x,0,360,1920,532);
        assert(next>=last && next<=1388); last=next;
    }
    puts("PASS minimap navigation: center, four edges, outside drag, resize, monotonic movement");
    return 0;
}
