@.\tools\sokol-shdc.exe --input assets/shader.glsl --output src/shader.h --slang hlsl5:glsl300es
@.\tools\sokol-shdc.exe --input assets/display.glsl --output src/display-shd.h --slang hlsl5:glsl300es
@zig cc -std=c2x -shared -g -o bin/client.dll build_client.c
