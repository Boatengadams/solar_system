@echo off
REM Portable launcher for BAGS_LAB (Windows).
REM Status: BUILD-READY launcher / package NOT YET TESTED.
REM Uses the launcher directory; no fixed drive letter required.
REM Puts the package directory first on PATH so adjacent DLLs resolve.
setlocal
set "HERE=%~dp0"
set "PATH=%HERE%;%PATH%"

if exist "%HERE%BAGS_LAB.exe" (
  set "APP=%HERE%BAGS_LAB.exe"
) else if exist "%HERE%..\..\dist\windows-x64\BAGS_LAB.exe" (
  set "APP=%HERE%..\..\dist\windows-x64\BAGS_LAB.exe"
) else if exist "%HERE%..\..\build-windows-x64\Release\BAGS_LAB.exe" (
  set "APP=%HERE%..\..\build-windows-x64\Release\BAGS_LAB.exe"
) else if exist "%HERE%..\..\build\Release\BAGS_LAB.exe" (
  set "APP=%HERE%..\..\build\Release\BAGS_LAB.exe"
) else if exist "%HERE%..\..\build\BAGS_LAB.exe" (
  set "APP=%HERE%..\..\build\BAGS_LAB.exe"
) else (
  echo BAGS_LAB.exe not found near launcher: %HERE%
  echo Build on Windows ^(MSVC preset windows-x64^) or place a package under dist\windows-x64\
  echo Do not claim Windows support until a native package exists.
  exit /b 1
)

"%APP%" %*
endlocal
