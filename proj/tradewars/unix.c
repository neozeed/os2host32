//linux or others...
//http://cboard.cprogramming.com/faq-board/27714-faq-there-getch-conio-equivalent-linux-unix.html
#include <termios.h>
#include <unistd.h>

struct termios oldt,newt;

void OSinit( ) {
  tcgetattr( STDIN_FILENO, &oldt );
  newt = oldt;
  //newt.c_lflag &= ~( ICANON | ECHO );
  newt.c_lflag &= ~( ICANON );
  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
  srand( (unsigned)time(NULL));	//seed the random number generator
}


void OSdinit() {
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
}
