@echo off
set DIFF="C:\Program Files\Git\usr\bin\diff.exe" --binary -q
set NASM="..\nasm.exe"

cl /Zi 002_decode.c
if errorlevel 1 goto fail

set fname=listing_0039_more_mov
002_decode < %fname% > %fname%_out.asm
%NASM% %fname%_out.asm
%DIFF% %fname% %fname%_out
if errorlevel 1 goto fail

set fname=listing_0040_even_more_mov
002_decode < %fname% > %fname%_out.asm
%NASM% %fname%_out.asm
%DIFF% %fname% %fname%_out
if errorlevel 1 goto fail

goto end
:fail
:end
