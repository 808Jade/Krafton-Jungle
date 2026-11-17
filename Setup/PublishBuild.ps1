<#
.SYNOPSIS
  배포·시연·제출용 빌드를 만들고, 소스 인덱싱한 뒤 exe·pdb를 심볼 서버(공유 폴더)에 올린다.

.DESCRIPTION
  순서가 중요하다. 소스 인덱싱을 먼저 해야 심볼 서버에 올라가는 PDB에 인덱스가 들어간다.
    1) 커밋하지 않은 변경이 없고, HEAD가 원격에 푸시돼 있는지 확인한다.
    2) 프로젝트를 다시 만들고(premake) 빌드한다. (MSBuild는 vswhere로 찾는다)
    3) Setup\SourceIndex.ps1로 PDB에 소스 인덱스를 쓴다.
    4) symstore로 exe·pdb를 심볼 서버에 올린다. (버전 메모 = 커밋 해시)
  업로드한 뒤에는 다시 빌드하지 말고 이 exe를 그대로 배포한다. 다시 빌드하면 PDB GUID·Age가 바뀌어 짝이 깨진다.

  두 가지로 쓴다.
    - 직접 실행: 위 1~4를 모두 한다. 조건이 안 맞으면 멈춘다.
    - Publish 구성의 빌드 후 단계 (-SkipBuild -FromBuild): 3~4만 한다. 조건이 안 맞거나 실패하면
      경고만 남기고 성공으로 끝낸다. (빌드를 깨지 않는다. 경고는 Visual Studio 오류 목록에 보인다)

  심볼 서버 경로는 -SymbolStore로 주거나, Setup\SymbolStore.txt의 첫 줄(#로 시작하면 주석)에서 읽는다.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File Setup\PublishBuild.ps1 -SymbolStore \\192.168.0.15\symbols

.EXAMPLE
  # 시험용: 커밋·푸시 검사 없이, 이미 빌드된 결과로 로컬 폴더에 올려 본다
  powershell -ExecutionPolicy Bypass -File Setup\PublishBuild.ps1 -SymbolStore C:\TestSymbols -SkipBuild -AllowUnpushed
#>
param(
	# 심볼 서버 경로. 예: \\192.168.0.15\symbols (Windows 공유 폴더). 비우면 Setup\SymbolStore.txt에서 읽는다.
	[string]$SymbolStore = "",
	[string]$Configuration = "Release",
	[string]$Project = "Source\Editor\HitoriEditor.vcxproj",
	[string]$ExeName = "HitoriEditor",
	[string]$ProductName = "HitoriEngine",
	# 이미 빌드된 결과를 그대로 쓴다.
	[switch]$SkipBuild,
	# 커밋하지 않은 변경·푸시하지 않은 커밋이 있어도 진행한다. (시험용. 이렇게 올린 소스 인덱스는 GitHub에서 받을 수 없다)
	[switch]$AllowUnpushed,
	# 빌드 후 단계에서 부를 때. 실패해도 빌드를 깨지 않고 경고로 끝낸다.
	[switch]$FromBuild
)
$ErrorActionPreference = 'Stop'

# 직접 실행이면 예외로 멈추고, 빌드 후 단계면 MSBuild 경고 형식으로 남기고 성공(0)으로 끝낸다.
function Stop-Publish([string]$Message)
{
	if ($FromBuild)
	{
		Write-Host "PublishBuild : warning PUB001: $Message"
		Write-Host "PublishBuild : warning PUB001: 심볼 서버 업로드를 건너뛴다. (빌드 결과는 그대로 쓸 수 있다)"
		exit 0
	}
	throw $Message
}

try
{
	$Root = (git rev-parse --show-toplevel).Replace('/', '\')
	Set-Location $Root
	$Commit = (git rev-parse HEAD).Trim()
	$Subject = (git log -1 --format=%s).Trim()

	if (-not $SymbolStore)
	{
		$ConfigFile = Join-Path $Root "Setup\SymbolStore.txt"
		if (Test-Path $ConfigFile)
		{
			$SymbolStore = Get-Content $ConfigFile | ForEach-Object { $_.Trim() } |
				Where-Object { $_ -and -not $_.StartsWith('#') } | Select-Object -First 1
		}
		if (-not $SymbolStore) { Stop-Publish "심볼 서버 경로가 없다. Setup\SymbolStore.txt에 \\IP\symbols 형식으로 적는다." }
	}

	# 1) 커밋·푸시 확인
	$Dirty = git status --porcelain --untracked-files=no
	$Pushed = git branch -r --contains $Commit
	if (($Dirty -or -not $Pushed) -and -not $AllowUnpushed)
	{
		if ($Dirty) { Write-Host "커밋하지 않은 변경:"; $Dirty | ForEach-Object { Write-Host "  $_" } }
		Stop-Publish $(if ($Dirty) { "커밋하지 않은 변경이 있다. 모두 커밋·푸시한 뒤 다시 빌드한다." } else { "HEAD($Commit)가 원격에 없다. 푸시한 뒤 다시 빌드한다." })
	}

	$SymStore = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\symstore.exe"
	if (-not (Test-Path $SymStore)) { Stop-Publish "symstore.exe가 없다: $SymStore (Windows SDK의 Debugging Tools for Windows를 설치한다)" }

	# 2) 빌드
	if (-not $SkipBuild)
	{
		& "$Root\Setup\premake5.exe" vs2026
		if ($LASTEXITCODE -ne 0) { throw "premake 실패" }
		$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
		$MSBuild = & $VsWhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
		if (-not $MSBuild) { throw "MSBuild를 찾지 못했다." }
		& $MSBuild $Project "-p:Configuration=$Configuration" -p:Platform=x64 -m -v:m -nologo
		if ($LASTEXITCODE -ne 0) { throw "빌드 실패" }
	}

	$OutDir = Join-Path $Root "Build\Bin\$Configuration-windows-x86_64"
	$Exe = Join-Path $OutDir "$ExeName.exe"
	$Pdb = Join-Path $OutDir "$ExeName.pdb"
	foreach ($File in @($Exe, $Pdb))
	{
		if (-not (Test-Path $File)) { Stop-Publish "빌드 결과가 없다: $File" }
	}

	# 심볼 서버(공유 폴더)에 닿는지 먼저 확인한다. 네트워크 경로가 틀리면 symstore가 오래 기다리다 실패한다.
	if (-not (Test-Path $SymbolStore))
	{
		if ($SymbolStore.StartsWith('\\')) { Stop-Publish "심볼 서버에 접근할 수 없다: $SymbolStore (공유 폴더·네트워크·권한을 확인한다)" }
		New-Item -ItemType Directory -Force $SymbolStore | Out-Null
	}

	# 3) 소스 인덱싱 (symstore보다 먼저)
	& "$Root\Setup\SourceIndex.ps1" -PdbPath $Pdb

	# 4) 심볼 서버 업로드. symstore가 <이름>\<식별자>\<이름> 구조로 버전별로 쌓는다.
	#    exe 식별자 = TimeDateStamp+SizeOfImage, pdb 식별자 = GUID+Age (덤프의 모듈 기록과 같은 값)
	foreach ($File in @($Exe, $Pdb))
	{
		& $SymStore add /f $File /s $SymbolStore /t $ProductName /v $Commit /c $Subject
		if ($LASTEXITCODE -ne 0) { throw "symstore 실패: $File" }
	}

	Write-Host ""
	Write-Host "Published $ProductName $Configuration @ $Commit"
	Write-Host "  exe  : $Exe"
	Write-Host "  pdb  : $Pdb (source indexed)"
	Write-Host "  store: $SymbolStore"
	Write-Host "이 exe를 그대로 배포한다. 다시 빌드하면 심볼 서버의 PDB와 짝이 맞지 않는다."
}
catch
{
	Stop-Publish $_.Exception.Message
}
