# megalinker
Linker to build of Megaroms for MSX using SDCC

This repository includes the code of the linker (megalinker.cc), and the suggested CRT and API used to manage the block exchange.

```
Usage: megalinker [OPTION] [ROM_FILE] [REL_FILES] [LIB_FILES]
  Option: -l N sets the debug level to N (default is 3)
  Option: -h prints this help message
  *.rom: the output rom file (only the last one counts)
  *.rel: any number of compiled relocatable files from sdcc. Only the required files will be used.
  *.lib: any number of sdcc library files that contain relocatable files from sdcc. Those are processed as if they were individually supplied to the linker
```

Unlike the default sdcc linker, you can pass any number of relocatable files to the linker and only the required ones will be included.

The Megalinker allows you to create programs that leverage the rom mapper of megaroms. However, this comes at a complexity cost: you can not freely call functions C file from different C files, as those might not be loaded at the moment.
To use any external symbol from a C file, its corresponding module (c file, or assembler file) must be loaded first in one of the 4 pages available: A, B, C or D.
Of course, loading a module into a page will deallocate the module previously allocated in such page, thus module loading must be performed carefully to prevent deallocating the current page.
The linker can detect some mistakes and show some warnings, but it can not completely prevent mistakes.

The megalinker supports any 8K mapper (configurable via the crt file), and will automatically allocate modules in segments, trying to fit as many modules as possible in the rom.

Functions labeled as `__nonbanked` will be copied into RAM during boot, and thus will be always available independently from the current segment mapping in the rom.
We suggest to declare function pointers and isr hooks as `__nonbanked`, to ensure its availability at all times.
Also `__nonbanked` trampolines can be used to interface between modules that are mapped to the same page.


Working with modules/megarom is somewhat different than working with libraries.
E.g. in fusion-C you want to place each function in a different relocatable file (i.e., module), so that the default linker will link only the required functions.
On the other hand, when using megalinker, you can not call trivially functions from different modules, thus splitting a file into many modules may be harmful.

To enable the use of already created libraries, we enable the option to manually combine several modules into a larger one, which will be placed in a single segment, and thus mapped as a single unit.
This is useful to link with, e.g., fusion-C. As seen in the example.


## Documentation

### Module names:

Each c file is compiled and generates a module named after the c file.
E.g., a file named "hello.c" will generate a module named hello.

Each assembler file should use the `.module` directive to name the module.
If a compile file does not have a module directive, the assembly will
try to assign one based on the file name, or the first defined symbol
in case the file name is not available (e.g., in libraries).

### Banked / Non Banked Symbols:

Each symbol declared in a source can be declared as banked, or non banked.
By default, all symbols are declared as banked unless the keyword 
`__nonbanked` is used. The main function must be declared as `__nonbanked`.

Banked symbols within a module are placed in the `_CODE` area and if used 
from a different module they must be requested and loaded before use, 
and can be placed in PAGE A, B, C or D. A module can only be placed in 
one of the four pages.

All symbols declared as `__nonbanked` will be placed in the `_HOME` area.
This area is copied into RAM on boot and will be always accessible.
`__nonbanked` is useful to place trampolines, isr functions, and small 
commonly used snippets.

### Usage rules

From | Target symbol | Usage
-----|---------------|----
`__nonbanked` | `__nonbanked` | always allowed
`__nonbanked` | `__banked` | must load target module first
`__banked` | `__banked`, same module | always allowed
`__banked` | `__banked`, different module, different page | must load target module first
`__banked` | `__banked`, different module, same page | not allowed

### Module combination:

Sometimes, you want different modules to be placed in the same segment, 
as if they were a single module. This is especially useful when using
external libraries whose code is split in different files (e.g. fusion-c).
Using `ML_MOVE_SYMBOLS_TO` you can move all symbols defined in one module
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
`ML_MOVE_SYMBOLS_TO(target_module, source_module)` | the symbols that were defined in `source_module` now belong to `target_module`. `source_module` shold not be requested now.
`ML_REQUEST_X(module)` | is a declaration that must be used prior to use a module.

Notes:
`ML_LOAD_MODULE_X(module)` is equivalent to `ML_LOAD_SEGMENT_X(ML_SEGMENT_X(module))`
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




