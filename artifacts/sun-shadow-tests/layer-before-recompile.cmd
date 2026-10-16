@call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
@cl.exe @"C:\Users\nanob\source\repos\DATura\artifacts\sun-shadow-tests\layer-before-compile.rsp"
@if errorlevel 1 exit /b 1
@link.exe @"C:\Users\nanob\source\repos\DATura\artifacts\sun-shadow-tests\layer-before-link.rsp"