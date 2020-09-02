    .module crt0_megalinker

; crt0 for MSX ROM of 32KB, starting at 0x4000
;------------------------------------------------

.globl  _main

.globl  ___ML_RAM_START

.globl  ___ML_INIT_ROM_START
.globl  ___ML_INIT_RAM_START
.globl  ___ML_INIT_SIZE

.globl  _ML_current_segment_a
.globl  _ML_current_segment_b
.globl  _ML_current_segment_c
.globl  _ML_current_segment_d
.globl  _ML_address_d
.globl  _ML_address_c
.globl  _ML_address_b
.globl  _ML_address_a



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
___ML_RAM_START =   0xC000

_ML_address_a =   0x5000
_ML_address_b =   0x7000
_ML_address_c =   0x9000
_ML_address_d =   0xb000
_ML_current_segment_a::
    .ds 1
_ML_current_segment_b::
    .ds 1
_ML_current_segment_c::
    .ds 1
_ML_current_segment_d::
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
    ld  (_ML_current_segment_a),a
    ld  (_ML_address_a),a
    ld  (_ML_address_a),a
    inc a
    ld  (_ML_current_segment_b),a
    ld  (_ML_address_b),a
    ld  (_ML_address_b),a
    inc a
    ld  (_ML_current_segment_c),a
    ld  (_ML_address_c),a
    ld  (_ML_address_c),a
    inc a
    ld  (_ML_current_segment_d),a
    ld  (_ML_address_d),a
    ld  (_ML_address_d),a

;   Sets the stack at the top of the memory.
    ld sp,(0xfc4a)

; Detection and set of ROM page 2 (0x8000 - 0xbfff)
; based on a snippet taken from: http://karoshi.auic.es/index.php?topic=117.msg1465
    ; Primary slot
    call RSLREG
    rrca
    rrca
    and #0x03
    ; Secondary slot
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
    and #0x0c
    or c
    ld h, #0x80
    call ENASLT 
    
    di
    
;   copies intial values to RAM
    ld de, #___ML_INIT_RAM_START
    ld hl, #___ML_INIT_ROM_START
    ld bc, #___ML_INIT_SIZE
	ldir

;   Disables HTIMI isr
    call _ML_reset_HTIMI
    
.area _NONE
.area _GSINIT
    nop

.area _GSFINAL

;   enables interruptions and calls main
    ei
    call    _main 
    
    jp      init


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
