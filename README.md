# megalinker
Linker replacement  for sdcc for easy building of Megaroms for MSX

## Documentation

### Module names:

Each c file is compiled and generates a module named after the c file.
E.g., a file named "hello.c" will generate a module named hello.

Each assembler file should use the .module directive to name the module.
If a compile file does not have a module directive, the assembly will
try to assign one based on the file name, or the first defined symbol
in case the file name is not available (e.g., in libraries).

### Banked / Non Banked Symbols:

Each symbol declared in a source can be declared as banked, or non banked.
By default, all symbols are declared as banked unless the keyword 
__nonbanked is used. The main function must be declared as __nonbanked.

Banked symbols within a module are placed in the __CODE area and if used 
from a different module they must be requested and loaded before use, 
and can be placed in PAGE A, B, C or D. A module can only be placed in 
one of the four pages.

All symbols declared as __nonbanked will be placed in the __HOME area.
This area is copied into RAM on boot and will be always accessible.
__nonbanked is useful to place trampolines, isr functions, and small 
commonly used snippets.

### Usage rules

From | Target symbol | Usage
-----|---------------|----
__nonbanked | __nonbanked | always allowed
__nonbanked | __banked | must load target module first
__banked | __banked, same module | always allowed
__banked | __banked, different module, different page | must load target module first
__banked | __banked, different module, same page | not allowed

### Module combination:

Sometimes, you want different modules to be placed in the same segment, 
as if they were a single module. This is especially useful when using
external libraries whose code is split in different files (e.g. fusion-c).
Using the ML_MOVE_SYMBOLS_TO you can move all symbols defined in one module
to another module.

## Suggested API

The linker functionality is split in three places.
The linker itself arranges all rel modules as needed, and replaces symbols ___ML_A_## module for an makeup address that actually is the segment that points to the requested module module.
The functionality to inteface with the megarom lies on the crt0.megalinker.s, and the megalinker.h,
Both files can be modified to address specific needs.

The suggested API is the following:

Macro | Usage
---------|-----
`ML_SEGMENT_X(module)` | linker time constant that represents the segment where a module resides. 
`ML_LOAD_SEGMENT_X(segment)` | loads a segment in page X, returns the previously loaded segment in that page.
`ML_LOAD_MODULE_X(module)` | loads a module in page X, returns the previously loaded segment in that page.
`ML_RESTORE_X(segment)` | loads the segment in page X.
`ML_EXECUTE_X(module, code)` | executes code from the named module.
`ML_MOVE_SYMBOLS_TO(target_module, source_module)` | the symbols that were defined in `source_module` now belong to `target_module`.
`ML_REQUEST_X(module)` | is a declaration that must be used prior to use a module.

Notes:
`ML_LOAD_MODULE_X(module)`{:.C} is equivalent to `ML_LOAD_SEGMENT_X(ML_SEGMENT_X(module))`
`ML_EXECUTE_X(module, code)` is equivalent to:
```C
do {
	ML_REQUEST_A(module); 
	uint8_t old = ML_LOAD_MODULE_A(module); 
	{ 
		code; 
	} 
	ML_RESTORE_A(old);
} while(0)
```



