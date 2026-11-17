<#
.SYNOPSIS
  PDB에 소스 인덱스(srcsrv 스트림)를 써 넣는다. (Git + GitHub용)

.DESCRIPTION
  덤프를 여는 디버거가 크래시 지점의 소스를 "이 빌드를 만든 커밋" 그대로 GitHub에서 받아 오게 한다.
    1) srctool -r 로 PDB가 참조하는 소스 파일 목록을 얻는다.
    2) 저장소에 커밋된 파일만 골라 "원래 경로*저장소 상대 경로" 표를 만든다. (SDK·CRT 헤더는 빠진다)
    3) pdbstr -w 로 PDB의 srcsrv 스트림에 쓴다.
  디버거는 SRCSRVTRG 주소(커밋 해시가 고정된 raw.githubusercontent.com 주소)에서 파일을 받는다.
  Windows SDK에 들어 있는 ssindex.cmd는 Git을 지원하지 않아서 이 스크립트로 대신한다.

  주의: 빌드한 커밋이 GitHub에 푸시돼 있어야 디버거가 소스를 받을 수 있다.
        커밋하지 않은 변경으로 빌드했다면 받아 온 소스와 덤프의 줄이 어긋날 수 있다.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File Setup\SourceIndex.ps1 -PdbPath Build\Bin\Release-windows-x86_64\HitoriEditor.pdb

.NOTES
  필요: Windows SDK의 Debugging Tools for Windows (srctool.exe, pdbstr.exe)
  확인: srctool.exe -n <pdb>  → 파일마다 trg: https://... 주소가 나오면 성공
#>
param(
	[Parameter(Mandatory = $true)][string]$PdbPath,
	# 비우면 git remote origin 주소에서 만든다. (https://github.com/<owner>/<repo>)
	[string]$RawUrlBase = ""
)
$ErrorActionPreference = 'Stop'

$SrcSrvDir = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\srcsrv"
$SrcTool = Join-Path $SrcSrvDir "srctool.exe"
$PdbStr = Join-Path $SrcSrvDir "pdbstr.exe"
foreach ($Tool in @($SrcTool, $PdbStr)) {
	if (-not (Test-Path $Tool)) { throw "도구가 없다: $Tool (Windows SDK의 Debugging Tools for Windows를 설치한다)" }
}
if (-not (Test-Path $PdbPath)) { throw "PDB가 없다: $PdbPath" }
$PdbPath = (Resolve-Path $PdbPath).Path

$Root = (git rev-parse --show-toplevel).Replace('/', '\').TrimEnd('\') + '\'
$Commit = (git rev-parse HEAD).Trim()

if (-not $RawUrlBase) {
	# https://github.com/owner/repo(.git) 또는 git@github.com:owner/repo(.git) → https://raw.githubusercontent.com/owner/repo
	$Origin = (git remote get-url origin).Trim()
	if ($Origin -notmatch 'github\.com[:/]([^/]+)/([^/]+?)(\.git)?$') { throw "GitHub 저장소 주소를 알 수 없다: $Origin (-RawUrlBase로 직접 지정한다)" }
	$RawUrlBase = "https://raw.githubusercontent.com/$($Matches[1])/$($Matches[2])"
}

if (-not (git branch -r --contains $Commit)) {
	Write-Warning "커밋 $Commit 이 원격에 없다. 푸시해야 디버거가 GitHub에서 소스를 받을 수 있다."
}
if (git status --porcelain --untracked-files=no) {
	Write-Warning "커밋하지 않은 변경이 있다. 이 빌드의 소스와 커밋 내용이 다를 수 있다."
}

# 저장소가 관리하는 파일. PDB 경로는 대소문자가 섞여 있을 수 있어 소문자 키로 찾고, 값은 실제 이름이다. (GitHub 주소는 대소문자를 구분한다)
$Tracked = @{}
git ls-files | ForEach-Object { $Tracked[$_.ToLower()] = $_ }

# 1) PDB가 참조하는 소스 파일 목록
$Sources = & $SrcTool -r $PdbPath | Where-Object { $_ -match '^[A-Za-z]:\\' }

# 2) srcsrv 스트림 작성
$Lines = New-Object System.Collections.Generic.List[string]
$Lines.Add("SRCSRV: ini ------------------------------------------------")
$Lines.Add("VERSION=2")
$Lines.Add("VERCTRL=http")
$Lines.Add("SRCSRV: variables ------------------------------------------")
$Lines.Add("SRCSRVTRG=$RawUrlBase/$Commit/%var2%")
$Lines.Add("SRCSRV: source files ---------------------------------------")
$Count = 0
foreach ($Src in $Sources) {
	if (-not $Src.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase)) { continue }  # 저장소 밖(SDK·CRT) 제외
	$Key = $Src.Substring($Root.Length).Replace('\', '/').ToLower()
	if (-not $Tracked.ContainsKey($Key)) { continue }                                     # 커밋되지 않은 파일 제외
	$Lines.Add("$Src*$($Tracked[$Key])")
	$Count++
}
$Lines.Add("SRCSRV: end ------------------------------------------------")

$StreamFile = [IO.Path]::Combine([IO.Path]::GetDirectoryName($PdbPath), "srcsrv.txt")
[IO.File]::WriteAllLines($StreamFile, $Lines)

# 3) PDB에 쓰기
& $PdbStr -w "-p:$PdbPath" -s:srcsrv "-i:$StreamFile"
if ($LASTEXITCODE -ne 0) { throw "pdbstr 실패 (exit $LASTEXITCODE)" }
Write-Host "Source indexed: $Count files -> $RawUrlBase/$Commit"
