powershell -Command "& { Get-ChildItem shaders | Where-Object { $_ -notmatch """.*\.spv""" -and ( !(Test-Path -path shaders\$_.spv) -or ((Get-ChildItem -Path shaders\$_).LastWriteTime -gt (Get-ChildItem -Path shaders\$_.spv).LastWriteTime) ) } } | Foreach-Object { glslangValidator -V shaders\$_ -o shaders\$_.spv }" >compileShaders.log
rem    -or (Get-ChildItem -Path shaders\$_ -LastWriteTime) -gt (Get-ChildItem -Path shaders\$_.spv -LastWriteTime)
rem  | Foreach-Object { glslangValidator -V shaders\$_ -o shaders\$_.spv }
