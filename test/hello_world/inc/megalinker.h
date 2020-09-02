#pragma once
#include <stdint.h>


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PUBLIC INTERFACE
//

#define ML_REQUEST_A(module) extern const uint8_t __ML_A_## module
#define ML_REQUEST_B(module) extern const uint8_t __ML_B_## module
#define ML_REQUEST_C(module) extern const uint8_t __ML_C_## module
#define ML_REQUEST_D(module) extern const uint8_t __ML_D_## module

#define ML_SEGMENT_A(module) ((const uint8_t)&__ML_A_ ## module)
#define ML_SEGMENT_B(module) ((const uint8_t)&__ML_B_ ## module)
#define ML_SEGMENT_C(module) ((const uint8_t)&__ML_C_ ## module)
#define ML_SEGMENT_D(module) ((const uint8_t)&__ML_D_ ## module)

#define ML_LOAD_SEGMENT_A(segment) __ML_LOAD_SEGMENT_A(segment);
#define ML_LOAD_SEGMENT_B(segment) __ML_LOAD_SEGMENT_B(segment);
#define ML_LOAD_SEGMENT_C(segment) __ML_LOAD_SEGMENT_C(segment);
#define ML_LOAD_SEGMENT_D(segment) __ML_LOAD_SEGMENT_D(segment);

#define ML_LOAD_MODULE_A(module) ML_LOAD_SEGMENT_A(ML_SEGMENT_A(module))
#define ML_LOAD_MODULE_B(module) ML_LOAD_SEGMENT_B(ML_SEGMENT_B(module))
#define ML_LOAD_MODULE_C(module) ML_LOAD_SEGMENT_C(ML_SEGMENT_C(module))
#define ML_LOAD_MODULE_D(module) ML_LOAD_SEGMENT_D(ML_SEGMENT_D(module))

#define ML_RESTORE_A(segment) __ML_RESTORE_A(segment);
#define ML_RESTORE_B(segment) __ML_RESTORE_B(segment);
#define ML_RESTORE_C(segment) __ML_RESTORE_C(segment);
#define ML_RESTORE_D(segment) __ML_RESTORE_D(segment);

#define ML_EXECUTE_A(module, code) do { ML_REQUEST_A(module); uint8_t old = ML_LOAD_MODULE_A(module); { code; } ML_RESTORE_A(old); } while (0)
#define ML_EXECUTE_B(module, code) do { ML_REQUEST_B(module); uint8_t old = ML_LOAD_MODULE_B(module); { code; } ML_RESTORE_B(old); } while (0)
#define ML_EXECUTE_C(module, code) do { ML_REQUEST_C(module); uint8_t old = ML_LOAD_MODULE_C(module); { code; } ML_RESTORE_C(old); } while (0)
#define ML_EXECUTE_D(module, code) do { ML_REQUEST_D(module); uint8_t old = ML_LOAD_MODULE_D(module); { code; } ML_RESTORE_D(old); } while (0)

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE IMPLEMENTATION
//

#ifdef __SDCC
    extern volatile uint8_t ML_current_segment_a;
    extern volatile uint8_t ML_current_segment_b;
    extern volatile uint8_t ML_current_segment_c;
    extern volatile uint8_t ML_current_segment_d;

    extern volatile uint8_t ML_address_a;
    extern volatile uint8_t ML_address_b;
    extern volatile uint8_t ML_address_c;
    extern volatile uint8_t ML_address_d;

	inline uint8_t __ML_LOAD_SEGMENT_A(uint8_t segment) { register uint8_t old = ML_current_segment_a; ML_address_a = ML_current_segment_a = segment; return old; }
	inline uint8_t __ML_LOAD_SEGMENT_B(uint8_t segment) { register uint8_t old = ML_current_segment_b; ML_address_b = ML_current_segment_b = segment; return old; }
	inline uint8_t __ML_LOAD_SEGMENT_C(uint8_t segment) { register uint8_t old = ML_current_segment_c; ML_address_c = ML_current_segment_c = segment; return old; }
	inline uint8_t __ML_LOAD_SEGMENT_D(uint8_t segment) { register uint8_t old = ML_current_segment_d; ML_address_d = ML_current_segment_d = segment; return old; }

	inline void __ML_RESTORE_A(uint8_t segment) { ML_address_a = ML_current_segment_a = segment; }
	inline void __ML_RESTORE_B(uint8_t segment) { ML_address_b = ML_current_segment_b = segment; }
	inline void __ML_RESTORE_C(uint8_t segment) { ML_address_c = ML_current_segment_c = segment; }
	inline void __ML_RESTORE_D(uint8_t segment) { ML_address_d = ML_current_segment_d = segment; }

#endif
