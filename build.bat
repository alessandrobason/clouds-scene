@call rebuild
@zig cc -std=c2x -o bin/cloud.exe build_host.c -lkernel32 -luser32 -lshell32 -lgdi32 -ld3d11
@REM @bin\cloud