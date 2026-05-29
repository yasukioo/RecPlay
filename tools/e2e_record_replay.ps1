param(
    [string]$BaseUrl = "http://localhost:8080",
    [string]$OutputPath = "data/flight1.rpcap",
    [string]$MulticastAddress = "239.1.1.1",
    [int]$Port = 5000,
    [double]$Speed = 1.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-RecPlayJson {
    param(
        [Parameter(Mandatory = $true)][string]$Method,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $false)][object]$Body
    )

    $uri = "$BaseUrl$Path"
    $params = @{
        Uri = $uri
        Method = $Method
    }
    if ($null -ne $Body) {
        $params.ContentType = "application/json"
        $params.Body = ($Body | ConvertTo-Json -Depth 8 -Compress)
    }
    return Invoke-RestMethod @params
}

Write-Host "RecPlay E2E flow"
Write-Host '1. Start tools/udp_ws_bridge.py and open tools/trajectory_viewer.html.'
Write-Host '2. Start tools/sim_aircraft.py when recording begins.'
Read-Host "Press Enter to start recording"

$scriptRoot = Split-Path -Parent $PSCommandPath
$repoRoot = Resolve-Path (Join-Path $scriptRoot "..")
$outputPathValue = $OutputPath
if (-not [System.IO.Path]::IsPathRooted($outputPathValue)) {
    $outputPathValue = Join-Path $repoRoot $outputPathValue
}
$outputDirectory = Split-Path -Parent $outputPathValue
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Invoke-RecPlayJson -Method Post -Path "/api/session/record" -Body @{
    output_path = $outputPathValue
    codec = "zstd"
    protocols = @("UDP")
    protocol_config = @{
        address = $MulticastAddress
        port = $Port
        interface = "0.0.0.0"
        channel_name = "adsb"
        topic = "aircraft"
    }
} | Out-Null

Write-Host "Recording started. Run the simulator now."
Write-Host "Polling /api/stats every second. Press Ctrl+C to abort."
try {
    while ($true) {
        $state = Invoke-RecPlayJson -Method Get -Path "/api/session/state"
        $stats = Invoke-RecPlayJson -Method Get -Path "/api/stats"
        Write-Host ("State={0} Packets={1} Drops={2}" -f $state.state, $stats.total_packets, $stats.total_drops)
        Start-Sleep -Seconds 1
    }
} catch {
    Write-Host "Stopped polling: $($_.Exception.Message)"
}

Invoke-RecPlayJson -Method Post -Path "/api/session/record/stop" | Out-Null
Write-Host "Recording stopped."
Read-Host "Press Enter to open playback"

Invoke-RecPlayJson -Method Post -Path "/api/session/playback/open" -Body @{
    file_path = $outputPathValue
    protocol_config = @{
        address = $MulticastAddress
        port = $Port
        interface = "0.0.0.0"
    }
} | Out-Null

Invoke-RecPlayJson -Method Post -Path "/api/session/playback/play" -Body @{
    speed = $Speed
} | Out-Null

Write-Host "Playback started at ${Speed}x."
Write-Host "Use /api/session/playback/stop to terminate."
Read-Host "Press Enter to stop playback"

Invoke-RecPlayJson -Method Post -Path "/api/session/playback/stop" | Out-Null
Write-Host "Playback stopped."
