@echo off
chcp 65001 > nul
rem 빌드된 HitoriEditor.exe/.pdb를 심볼 창고에 등록한다.
rem 내 PC를 떠나는 빌드(배포, 시연, 팀원 전달)에만 실행한다.
rem 사용법: PublishSymbols.bat [Debug|Release|Executable ...] ["용도"]  (기본값: Release)
rem 용도를 인자로 주지 않으면 실행 중에 입력받는다.
setlocal
cd /d "%~dp0"

rem ① 구성
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "BIN_DIR=Build\Bin\%CONFIG%-windows-x86_64"

rem ①-2 용도 (인자가 없으면 입력받고, Enter만 누르면 생략)
set "NOTE=%~2"
if "%NOTE%"=="" set /p "NOTE=용도 (예: 11/14 중간 시연): "
set "COMMENT=%CONFIG%"
if not "%NOTE%"=="" set "COMMENT=%CONFIG% - %NOTE%"

rem ② 도구와 결과물 확인
set "SYMSTORE=%ProgramFiles(x86)%\Windows Kits\10\Debuggers\x64\symstore.exe"
if not exist "%SYMSTORE%" goto :NoSymstore
if not exist "%BIN_DIR%\HitoriEditor.pdb" goto :NoBuild

rem ③ 창고 경로 (팀 공유 폴더를 쓰려면 HITORI_SYMBOL_STORE 환경 변수를 설정)
set "STORE=%~dp0Saved\Symbols"
if defined HITORI_SYMBOL_STORE set "STORE=%HITORI_SYMBOL_STORE%"
if not exist "%STORE%" mkdir "%STORE%"

rem ④ 커밋 해시 (커밋 안 된 변경이 있으면 -dirty 표시)
set "COMMIT=unknown"
for /f %%H in ('git rev-parse --short HEAD 2^>nul') do set "COMMIT=%%H"
git diff --quiet HEAD 2>nul || set "COMMIT=%COMMIT%-dirty"

rem ⑤ 등록
echo [심볼 등록] %COMMENT% / %COMMIT%
"%SYMSTORE%" add /f "%BIN_DIR%\HitoriEditor.exe" /s "%STORE%" /t Hitori /v "%COMMIT%" /c "%COMMENT%"
if errorlevel 1 goto :Fail
"%SYMSTORE%" add /f "%BIN_DIR%\HitoriEditor.pdb" /s "%STORE%" /t Hitori /v "%COMMIT%" /c "%COMMENT%"
if errorlevel 1 goto :Fail

echo 완료: %STORE%
PAUSE
exit /b 0

:NoSymstore
echo [오류] symstore.exe가 없다. Windows SDK에 Debugging Tools for Windows를 설치한다.
PAUSE
exit /b 1

:NoBuild
echo [오류] %BIN_DIR%에 빌드 결과물이 없다. %CONFIG% 구성으로 먼저 빌드한다.
PAUSE
exit /b 1

:Fail
echo [오류] symstore 등록에 실패했다.
PAUSE
exit /b 1