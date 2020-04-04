@echo off
rem *************++++++++++++++>>>>>>>>>> USE "Developer Command Prompt for VS 2017" <<<<<<<<<<<<+++++++++++++++++++*******************
set cmake="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set msbuild="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\amd64\msbuild.exe"
if "%1" equ "clean" (
	rmdir /S /Q build
	timeout /T 3
	mkdir build || goto fail
) else (
	mkdir build 2>&1 >nul
)
cd build || goto fail
%cmake% -G "Visual Studio 15 2017 Win64" %* .. || goto end
%msbuild% ALL_BUILD.vcxproj /p:Platform=x64 /p:Configuration=Debug /verbosity:quiet 2>&1 >build.log || goto end
cd Debug
echo -n executing from
cd
rem echo ** v main v **
main.exe || echo fail
echo log:
cat runtime.log
cd ..
:end:
cd ..
:fail:
