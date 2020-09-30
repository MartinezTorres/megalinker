#include <megalinker.h>

static char chget() __z88dk_fastcall;
static void chput(char c) __z88dk_fastcall;

static inline void asm_placeholder() {
	
// CHGET
// Address  : #009F
// Function : One character input (waiting)
// Output   : A  - ASCII code of the input character
// Registers: AF
	
  	__asm
_chget::
	call 0x009F      ; call CHGET
    ld l, a 
    ret
	__endasm;	

// CHPUT
// Address  : #00A2
// Function : Displays one character
// Input    : A  - ASCII code of character to display

  	__asm
_chput::
    ld a, l
	call 0x00A2      ; call CHPUT
	ret
	__endasm;	
}

static void puts(const char *str) __z88dk_fastcall {
	
    while (*str) chput(*str++);
}


inline void wait_frame() {

  	__asm
  	ei
    halt
	__endasm;	
}

static const char *hello_world_str_1 = "Hello\n";

static void main_banked() {

    puts(hello_world_str_1);	
    
	static const char *hello_world_str_2 = " MSX World!\n";

    puts(hello_world_str_2);
    
    for (;;) wait_frame();
}

int main(void) __nonbanked { ML_EXECUTE_A(main, main_banked() ); return 0; }

