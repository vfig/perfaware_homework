@echo off
set DIFF="C:\Program Files\Git\usr\bin\diff.exe" -q

cl 001_decode.c
if errorlevel 1 goto fail

set fname=listing_0037_single_register_mov
001_decode < %fname% > %fname%_out.asm
nasm %fname%_out.asm
%DIFF% %fname% %fname%_out
if errorlevel 1 goto fail

set fname=listing_0038_many_register_mov
001_decode < %fname% > %fname%_out.asm
nasm %fname%_out.asm
%DIFF% %fname% %fname%_out
if errorlevel 1 goto fail

goto end
:fail
:end
