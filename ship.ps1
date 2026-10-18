# Builds crittercam and publishes it as a release in the private GitHub repo, where every installed copy picks it up.
#   .\ship.ps1 0.2.0 "what changed"
# Needs (once, on this PC): Visual Studio 2022, Rust, Node, gh logged in, the signing key in %USERPROFILE%\.tauri\crittercam.key,
# and ui\src-tauri\update-token.txt with a read-only fine-grained token for this repo (Contents: Read). See README "Updates".
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Notes = ""
)
$ErrorActionPreference = "Stop"
$repo = "gaknippel/crittercam"
$root = $PSScriptRoot
$ui = Join-Path $root "ui"
$tokenFile = Join-Path $ui "src-tauri\update-token.txt"
$keyFile = Join-Path $env:USERPROFILE ".tauri\crittercam.key"

if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Version must look like 1.2.3" }
if (-not (Test-Path $tokenFile) -or -not "$(Get-Content $tokenFile -Raw)".Trim()) { throw "Paste a read-only GitHub token into $tokenFile first (see README, Updates)" }
if (-not (Test-Path $keyFile)) { throw "Missing signing key $keyFile" }
if (Get-Process nightreign -ErrorAction SilentlyContinue) { throw "Close the game first: it keeps crittercam.dll open, so it can't be rebuilt" }

# files are written without a BOM: the json parsers on the other end don't accept one
function Set-Text($path, $text) { [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding $false)) }
function Set-Version($path, $pattern, $replacement) {
    $text = [IO.File]::ReadAllText($path)
    $new = [regex]::new($pattern, 'Multiline').Replace($text, $replacement, 1)
    if ($new -eq $text -and $text -notmatch [regex]::Escape($Version)) { throw "Couldn't set the version in $path" }
    Set-Text $path $new
}

Write-Host "== version $Version"
Set-Version (Join-Path $ui "src-tauri\tauri.conf.json") '"version": "[^"]*"' "`"version`": `"$Version`""
Set-Version (Join-Path $ui "src-tauri\Cargo.toml") '^version = "[^"]*"' "version = `"$Version`""
Set-Version (Join-Path $ui "package.json") '"version": "[^"]*"' "`"version`": `"$Version`""

Write-Host "== camera dll"
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
& $msbuild (Join-Path $root "crittercam.sln") /t:crittercam /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "dll build failed" }

Write-Host "== app installer"
$env:CRITTERCAM_UPDATE_TOKEN = (Get-Content $tokenFile -Raw).Trim()
$env:TAURI_SIGNING_PRIVATE_KEY = Get-Content $keyFile -Raw
Push-Location $ui
try {
    # --ci: the key has no password, and PowerShell can't hold an empty env var (setting "" deletes it), so without this tauri prompts
    npm run tauri build -- --ci
    if ($LASTEXITCODE -ne 0) { throw "app build failed" }
} finally {
    Pop-Location
    Remove-Item Env:CRITTERCAM_UPDATE_TOKEN, Env:TAURI_SIGNING_PRIVATE_KEY
}
$setup = Join-Path $ui "src-tauri\target\release\bundle\nsis\crittercam_${Version}_x64-setup.exe"
if (-not (Test-Path "$setup.sig")) { throw "No signed installer at $setup" }

Write-Host "== release v$Version"
gh release create "v$Version" $setup --repo $repo --title "crittercam $Version" --notes $Notes
if ($LASTEXITCODE -ne 0) { throw "gh release create failed" }
# the manifest points at the installer's api url (private assets can only be downloaded through the api with the token)
$setupName = Split-Path $setup -Leaf
# filtered here, not with --jq: Windows PowerShell strips the inner quotes a jq filter needs when passing it to gh
$assetUrl = ((gh api "repos/$repo/releases/tags/v$Version" | ConvertFrom-Json).assets | Where-Object { $_.name -eq $setupName }).url
if (-not $assetUrl) { throw "Couldn't find the uploaded installer" }
$manifest = [ordered]@{
    version   = $Version
    notes     = $Notes
    pub_date  = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    platforms = @{ "windows-x86_64" = @{ signature = (Get-Content "$setup.sig" -Raw).Trim(); url = $assetUrl } }
}
$manifestFile = Join-Path $env:TEMP "latest.json"
Set-Text $manifestFile ($manifest | ConvertTo-Json -Depth 5)
gh release upload "v$Version" $manifestFile --repo $repo --clobber
if ($LASTEXITCODE -ne 0) { throw "uploading latest.json failed" }

Write-Host "== done: v$Version is live. Installed copies offer it on their next start. Commit the version bump."
Write-Host "   first install on a new PC: gh release download v$Version --repo $repo --pattern '*setup.exe'"
