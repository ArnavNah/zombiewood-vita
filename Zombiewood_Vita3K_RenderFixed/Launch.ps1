param([string]$Vita3KPath)
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
if(-not $Vita3KPath){$Vita3KPath=Join-Path $workspace 'audit/runtime/ZombiewoodAudit.exe'}
if(-not (Test-Path -LiteralPath $Vita3KPath)){throw 'Pass -Vita3KPath with the full path to Vita3K.exe.'}
$running=Get-Process Vita3K,ZombiewoodAudit -ErrorAction SilentlyContinue
if($running){throw 'Close the existing Vita3K window before launching this build.'}
$runtime=Split-Path -Parent $Vita3KPath
$config=Join-Path $runtime 'config.yml'
$pref=Join-Path $env:APPDATA 'Vita3K/Vita3K'
$configText=''
if(Test-Path -LiteralPath $config){
    $configText=[IO.File]::ReadAllText($config)
    $match=[regex]::Match($configText,'(?m)^pref-path:\s*(.+)$')
    if($match.Success -and $match.Groups[1].Value -notmatch 'audit[\\/]mission-loading[\\/]storage'){
        $pref=$match.Groups[1].Value.Trim().Trim('"')
    }
    $pref=$pref.TrimEnd('\','/')
    $backup=Join-Path $runtime 'config.before-controller-fix.yml'
    if(-not (Test-Path -LiteralPath $backup)){Copy-Item -LiteralPath $config -Destination $backup}
    $configText=[regex]::Replace($configText,'(?m)^memory-mapping:.*$','memory-mapping: disabled')
    $configText=[regex]::Replace($configText,'(?m)^backend-renderer:.*$','backend-renderer: Vulkan')
    if($configText -match '(?m)^pref-path:'){
        $configText=[regex]::Replace($configText,'(?m)^pref-path:.*$',('pref-path: '+$pref+'\'))
    }else{$configText+="`n"+'pref-path: '+$pref+'\'+"`n"}
    [IO.File]::WriteAllText($config,$configText)
}
$app=Join-Path $pref 'ux0/app/ZOMBW0001'
New-Item -ItemType Directory -Path $app -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $PSScriptRoot 'Zombiewood.vpk'))
try {
    $appRoot=[IO.Path]::GetFullPath($app)+[IO.Path]::DirectorySeparatorChar
    foreach($entry in $archive.Entries){
        if($entry.FullName.EndsWith('/')){continue}
        $target=[IO.Path]::GetFullPath((Join-Path $app $entry.FullName))
        if(-not $target.StartsWith($appRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe VPK entry.'}
        if(Test-Path -LiteralPath $target){
            if($entry.FullName -ne 'eboot.bin'){continue}
            $backup=Join-Path $app 'eboot.before-controller-fix.bin'
            if(-not (Test-Path -LiteralPath $backup)){Copy-Item -LiteralPath $target -Destination $backup}
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
    }
} finally {$archive.Dispose()}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'render-fixed-release.bin') -Destination (Join-Path $app 'render-fixed-release.bin') -Force
$source=Join-Path $PSScriptRoot 'data/zombiewood-audit'
$destination=Join-Path $pref 'ux0/data/zombiewood-audit'
foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse){
    $relative=$file.FullName.Substring($source.Length).TrimStart('\','/')
    $target=Join-Path $destination $relative
    if(Test-Path -LiteralPath $target){continue}
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
Write-Host 'Starting Zombiewood. First launch compiles shaders; allow about a minute.'
Start-Process -FilePath $Vita3KPath -ArgumentList '-r ZOMBW0001 -S render-fixed-release.bin --keep-config' -WorkingDirectory $runtime -WindowStyle Normal
