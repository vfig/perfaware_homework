@echo off
set DIFF="C:\Program Files\Git\usr\bin\diff.exe" --binary -q
set NASM="..\nasm.exe"
set LISTINGS=..\listings
set name=001_decode

cl /nologo /Zi %name%.c
if errorlevel 1 goto fail

for %%f in (
    listing_0037_single_register_mov
    listing_0038_many_register_mov
) do (
    set fname=%%f
    %name%.exe < %LISTINGS%\%fname% > %fname%_out.asm
    %NASM% %fname%_out.asm
    %DIFF% %LISTINGS%\%fname% %fname%_out
    if errorlevel 1 goto fail
)

goto end
:fail
:end
