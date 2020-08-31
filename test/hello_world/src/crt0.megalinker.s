    .module crt0_megalinker

; crt0 for MSX ROM of 32KB, starting at 0x4000
; includes detection and set of ROM page 2 (0x8000 - 0xbfff)
; suggested options: --code-loc 0x4020 --data-loc 0xc000

.globl  _main

.globl  ___ML_START_RAM

.globl  ___ML_HOME_ROM_START
.globl  ___ML_HOME_RAM_START
.globl  ___ML_HOME_SIZE

.globl  ___ML_INITIALIZER_START
.globl  ___ML_INITIALIZED_START
.globl  ___ML_INITIALIZED_SIZE

.globl  ___ML_GSINIT

.globl  ___ML_current_segment_a
.globl  ___ML_current_segment_b
.globl  ___ML_current_segment_c
.globl  ___ML_current_segment_d
.globl  ___ML_address_d
.globl  ___ML_address_c
.globl  ___ML_address_b
.globl  ___ML_address_a



.area _DATA
;--------------------------------------------------------
; MSX BIOS CALLS
;--------------------------------------------------------
ENASLT = 0x0024
RSLREG = 0x0138

;--------------------------------------------------------
; MSX BIOS WORK AREA
;--------------------------------------------------------
HIMEM = 0xFC4A
EXPTBL = 0xFCC1

;--------------------------------------------------------
; MSX BIOS SYSTEM HOOKS
;--------------------------------------------------------
HTIMI = 0xFD9F

;--------------------------------------------------------
; DATA
;--------------------------------------------------------
.area _DATA
___ML_address_a =   0x5000
___ML_address_b =   0x7000
___ML_address_c =   0x9000
___ML_address_d =   0xb000
___ML_current_segment_a::
    .ds 1
___ML_current_segment_b::
    .ds 1
___ML_current_segment_c::
    .ds 1
___ML_current_segment_d::
    .ds 1

;--------------------------------------------------------
; HEADER
;--------------------------------------------------------

.area _HEADER (ABS)
; Reset vector
    .org 0x4000
    .db  0x41
    .db  0x42
    .dw  init
    .dw  0x0000
    .dw  0x0000
    .dw  0x0000
    .dw  0x0000
    .dw  0x0000
    .dw  0x0000
;
;   .ascii "END ROMHEADER"
;

init:
;   Disables Interruptions
    di

;   We initialize the mapper repeatedly, to trigger correctly megaflashrom and openmsx mapper detection.
    xor a
    ld  (___ML_current_segment_a),a
    ld  (___ML_address_a),a
    ld  (___ML_address_a),a
    inc a
    ld  (___ML_current_segment_b),a
    ld  (___ML_address_b),a
    ld  (___ML_address_b),a
    inc a
    ld  (___ML_current_segment_c),a
    ld  (___ML_address_c),a
    ld  (___ML_address_c),a
    inc a
    ld  (___ML_current_segment_d),a
    ld  (___ML_address_d),a
    ld  (___ML_address_d),a

;   Sets the stack at the top of the memory.
    ld sp,(0xfc4a)

;   Finds second rom page
    call find_rom_page_2

;   Disables HTIMI isr
    call _ML_reset_HTIMI

;   Initializes RAM between 0xC000 and the top of memory to zero
    ; we calculate the size of the ram to be cleared ( (HIMEM) - #___ML_START_RAM - 1 ) and place it in bc
    ld de, #___ML_START_RAM+1
    ld hl,(HIMEM) 
    xor a
    sbc hl, de
    ld b, h
    ld c, l
    
    ; we do one iteration of cleaning
    ld hl, #___ML_START_RAM
    xor (hl)
    
    ; and call memcpy
    call _ML_memcpy

;   copies the code that must reside in RAM to the RAM area
    ld de, #___ML_HOME_RAM_START
    ld hl, #___ML_HOME_ROM_START
    ld bc, #___ML_HOME_SIZE
    call _ML_memcpy

;   copies intial values to RAM
    ld de, #___ML_INITIALIZED_START
    ld hl, #___ML_INITIALIZER_START
    ld bc, #___ML_INITIALIZED_SIZE
    call _ML_memcpy
    
;   calls C initialization routines
    call ___ML_GSINIT

;   enables interruptions and calls main
    ei
    call    _main 
    
    jp      init


; Ordering of segments for the linker.

;------------------------------------------------
; find_rom_page_2
; original name     : LOCALIZAR_SEGUNDA_PAGINA
; Original author   : Eduardo Robsy Petrus
; Snippet taken from: http://karoshi.auic.es/index.php?topic=117.msg1465
;
; Rutina que localiza la segunda pagina de 16 KB
; de una ROM de 32 KB ubicada en 4000h
; Basada en la rutina de Konami-
; Compatible con carga en RAM
; Compatible con expansores de slots
;------------------------------------------------
; Comprobacion de RAM/ROM

find_rom_page_2:
    ld hl, #0x4000
    ld b, (hl)
    xor a
    ld (hl), a
    ld a, (hl)
    or a
    jr nz,5$ ; jr nz,@@ROM
    ; El programa esta en RAM - no buscar
    ld (hl),b
    ret
5$: ; ----------- @@ROM:
    di
    ; Slot primario
    call RSLREG ; call RSLREG
    rrca
    rrca
    and #0x03
    ; Slot secundario
    ld c, a
    ld hl, #EXPTBL
    add a, l
    ld l, a
    ld a, (hl)
    and #0x80
    or c
    ld c, a
    inc l
    inc l
    inc l
    inc l
    ld a, (hl)
    ; Definir el identificador de slot
    and #0x0c
    or c
    ld h, #0x80
    ; Habilitar permanentemente
    call ENASLT 
    ei
    ret
;------------------------------------------------

;--------------------------------------------------------
; HOME
;--------------------------------------------------------

    .area   _HOME
    
___sdcc_call_hl::
    jp  (hl)
    
___sdcc_call_ix::
    jp  (ix)
    
___sdcc_call_iy::
    jp  (iy)
    
; no arguments needed.
; A is modified
_ML_reset_HTIMI::
    ld a,#0xC9 ; opcode for RET
    ld (HTIMI),a
    ret

; a non bankable function address is in HL.
; a is modified
_ML_set_HTIMI::
    ; Set new ISR vector
    ld a,#0xC3 ; opcode for JP
    ld (HTIMI),a
    ld (HTIMI+1),hl
    ret

; de is target
; hl is source
; bc is size 
; a is modified
_ML_memcpy::
	ld	a, b
	or	a, c
	ret	Z
	ldir
    ret
