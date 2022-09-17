# UNIX shell implemented as a project at university in 2017

Shell implements following functionalities:
- running programs (ls, cat, less, etc.) 
- running builtin commands (lcd, lkill, lls), doing the same things as their counterparts withut leading "l"
- handling input / output redirections
- handling pipes
- running background processes (invoked using "&" at the end) and writing them down when they are finished

The project only contains the files implemented by myself. It depends on additional files that were provided at university (e.g. for input parsing) and since I don't have the permission for sharing them, they are not included in the repository.

## Files
Project consists of several files, that are briefly described below:
- [mshell.c](mshell.c): contains almost all the functionalities mentioned above, except for builtin commands
- [builtins.c](builtins.c): contains the implementation of builtin commands
- [include/mshell.h](mshell.h): file with function declarations for [mshell.c](mshell.c)
- [mshell](mshell): compiled binary for demonstration how the program works				
