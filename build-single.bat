@.\tools\sokol-shdc.exe --input assets/shader.glsl --output src/shader.h --slang hlsl5:glsl300es
@.\tools\sokol-shdc.exe --input assets/display.glsl --output src/display-shd.h --slang hlsl5:glsl300es
@zig cc -std=c2x -O3 -o bin/clouds.exe build_single.c -lkernel32 -luser32 -lshell32 -lgdi32 -ld3d11
