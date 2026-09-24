# build_java.ps1 — compile Void Maiz's Java shim (android/java/) into classes.dex.
#
# The one Java a Void Maiz APK carries is org.voidmaiz.MaizActivity: the system
# keyboard's side of the text-input holiday (okf/concepts/text-input.md). A host's
# APK script calls this, then adds the dex at the root of the APK:
#
#   & "$VoidMaiz\android\build_java.ps1" -OutDir "$here\build\dex"
#   aapt add <apk> classes.dex        (run from -OutDir)
#
# and names org.voidmaiz.MaizActivity in its manifest, with hasCode="true".
# No Gradle: javac from Android Studio's JBR against the platform's android.jar,
# then d8 from build-tools. Same toolchain the APK scripts already require.
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [int]$MinApi = 26
)
$ErrorActionPreference = "Stop"

$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$bt = Get-ChildItem "$sdk\build-tools" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$platform = Get-ChildItem "$sdk\platforms" -Directory |
    Sort-Object { [double]($_.Name -replace "android-", "") } -Descending | Select-Object -First 1
$jar = "$($platform.FullName)\android.jar"
$jbr = "C:\Program Files\Android\Android Studio\jbr"
if (-not (Test-Path "$jbr\bin\javac.exe")) { throw "javac not found in $jbr (Android Studio's JBR)" }

$src = @(Get-ChildItem "$PSScriptRoot\java" -Recurse -Filter *.java | ForEach-Object { $_.FullName })
$classes = Join-Path $OutDir "classes"
Remove-Item -Recurse -Force $classes -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $classes | Out-Null

# --release 11 takes java.* from the JDK's own record of Java 11, and android.*
# from the classpath: the platform's API, nothing newer than the device has.
& "$jbr\bin\javac.exe" --release 11 -classpath $jar -d $classes "-Xlint:-options" @src
if ($LASTEXITCODE) { throw "javac failed" }

$env:JAVA_HOME = $jbr
$class_files = @(Get-ChildItem $classes -Recurse -Filter *.class | ForEach-Object { $_.FullName })
& "$($bt.FullName)\d8.bat" --release --min-api $MinApi --lib $jar --output $OutDir @class_files
if ($LASTEXITCODE) { throw "d8 failed" }
Write-Host "classes.dex: $(Join-Path $OutDir 'classes.dex') ($((Get-Item (Join-Path $OutDir 'classes.dex')).Length) bytes)"
