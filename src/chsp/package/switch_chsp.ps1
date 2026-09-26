param([Parameter(Mandatory=$true)][ValidateSet('Enable', 'Disable')][string]$Mode)
$ErrorActionPreference = 'Stop'

function Get-Hash([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try {
        return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $algorithm.Dispose()
    }
}

$lockStream = $null
$stage = $null
$saved = $null
try {
    # Serialize concurrent switch operations. The empty lock file may remain.
    $lockStream = [IO.File]::Open((Join-Path $PSScriptRoot 'chsp-switch.lock'),
        [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $manifestPath = Join-Path $PSScriptRoot 'chsp-package.json'
    $chspExpectedHash = $null
    if (Test-Path -LiteralPath $manifestPath) {
        try {
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            if ($manifest.files -and $manifest.files.'hspcmp_chsp.dll') {
                $chspExpectedHash = $manifest.files.'hspcmp_chsp.dll'
            }
        } catch { }
    }

    $active = Join-Path $PSScriptRoot 'hspcmp.dll'
    $backup = Join-Path $PSScriptRoot 'hspcmp_original.dll'
    $chsp = Join-Path $PSScriptRoot 'hspcmp_chsp.dll'

    if (-not (Test-Path -LiteralPath $active)) {
        throw 'hspcmp.dll is missing. Please extract cHSP into your HSP directory.'
    }
    if (-not (Test-Path -LiteralPath $chsp)) {
        throw 'hspcmp_chsp.dll is missing.'
    }

    $activeHash = Get-Hash $active
    $chspHash = Get-Hash $chsp
    if ($chspExpectedHash -and $chspHash -ne $chspExpectedHash) {
        throw 'hspcmp_chsp.dll does not match the package manifest (damaged file).'
    }

    $hasBackup = Test-Path -LiteralPath $backup

    if ($Mode -eq 'Enable') {
        if ($activeHash -eq $chspHash) {
            if (-not $hasBackup) {
                throw 'cHSP is active but its delegate DLL (hspcmp_original.dll) is missing.'
            }
            Write-Output 'cHSP is already enabled.'
            exit 0
        }
        $source = $chsp
        $expected = $chspHash
    } else {
        if ($activeHash -ne $chspHash) {
            Write-Output 'The original compiler is already enabled.'
            exit 0
        }
        if (-not $hasBackup) {
            throw 'Cannot restore: hspcmp_original.dll is missing.'
        }
        $backupHash = Get-Hash $backup
        $source = $backup
        $expected = $backupHash
    }

    $stage = Join-Path $PSScriptRoot ('chsp-switch-' + [guid]::NewGuid().ToString('N') + '.tmp')
    Copy-Item -LiteralPath $source -Destination $stage
    if ((Get-Hash $stage) -ne $expected) { throw 'Staged DLL verification failed.' }
    if ($Mode -eq 'Enable' -and -not $hasBackup) {
        # Keep this filename: the proxy loads it as its delegate.
        Move-Item -LiteralPath $active -Destination $backup
        $saved = $backup
    } else {
        $saved = Join-Path $PSScriptRoot ('chsp-switch-' + [guid]::NewGuid().ToString('N') + '.old')
        Move-Item -LiteralPath $active -Destination $saved
    }
    try {
        Move-Item -LiteralPath $stage -Destination $active
        $stage = $null
    } catch {
        Move-Item -LiteralPath $saved -Destination $active
        $saved = $null
        throw
    }
    if ($saved -ne $backup) { Remove-Item -LiteralPath $saved }
    $saved = $null
    Write-Output "$Mode completed. Restart the HSP editor."
} catch {
    Write-Output ("ERROR: " + $_.Exception.Message)
    Write-Output 'Close all HSP editors before switching. Keep hspcmp_original.dll.'
    exit 1
} finally {
    if ($stage -and (Test-Path -LiteralPath $stage)) { Remove-Item -LiteralPath $stage }
    if ($lockStream) { $lockStream.Dispose() }
}
