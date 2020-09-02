#include <megalinker.h>

static void CHPUT(char c) __z88dk_fastcall;

inline void wait_frame() {

  	__asm
  	ei
    halt
	__endasm;	
}
	

static void asm_placeholder() __naked {
    
  	__asm
_CHPUT:
    ld a, l
	jp 0x00A2      ; call CHPUT
	__endasm;	
}

static void puts(const char *str) __z88dk_fastcall {
	
    while (*str) CHPUT(*str++);
}


static const char *hello_world_str_1 = "Hello\n";

static void main_banked() {

    puts(hello_world_str_1);	
    
	static const char *hello_world_str_2 = " MSX World!\n";

    puts(hello_world_str_2);
    
    for (;;) wait_frame();
}

int main(void) __nonbanked { ML_EXECUTE_A(main, main_banked() ); return 0; }

