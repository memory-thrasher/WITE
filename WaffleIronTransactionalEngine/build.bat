setlocal EnableDelayedExpansion
set f=%1
if "%f%"=="" (
   set buildall=1
)
rem TODO if any header is nodified, set buildall
if "%buildall%"=="1" (
  echo >build.log
  set sources=
  for %%f in (*.cpp) do (
    clang++ --std=c++17 %%f -Wall -Wno-pragma-pack -o %%~nf.o -c -I"%VULKAN_SDK%\Include" -I"%VULKAN_SDK%\Third-Party\Include" >>build.log 2>&1 || goto end
    set sources=!sources! %%f
  )
  clang++ --std=c++17 %sources% -Wall -o WITE.dll >>build.log 2>&1 --for-linker "/SUBSYSTEM:WINDOWS" -L"%VULKAN_SDK%\Lib" -lvulkan-1 -L"%VULKAN_SDK%\Third-Party\Bin" -lSDL2
) else (
  clang++ --std=c++17 %1 -Wall -Wno-pragma-pack -o %~n1.o -c -I"%VULKAN_SDK%\Include" -I"%VULKAN_SDK%\Third-Party\Include" >build.log 2>&1
  del WITE.dll
)

:end:

