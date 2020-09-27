//
// Fusion-C
// My First Program in C
//

#include "header/msx_fusion.h"
#include "megalinker.h"

ML_MOVE_SYMBOLS_TO(fusion_c, print);
ML_MOVE_SYMBOLS_TO(fusion_c, printchar);

void main_banked(void) 
{
	ML_EXECUTE_B(fusion_c, Print("Hello MSX world !"));
	for (;;);
}
 
int main(void) __nonbanked { ML_EXECUTE_A(test, main_banked() ); return 0; }
