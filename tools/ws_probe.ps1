# Probe WebSocket /api/ws and bucket messages by type for N seconds.
param([int]$Seconds = 6)

Add-Type -AssemblyName System.Net.Http
$uri = [Uri]"ws://localhost:8080/api/ws"
$client = [System.Net.WebSockets.ClientWebSocket]::new()
$cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds($Seconds + 2))
try {
    $client.ConnectAsync($uri, $cts.Token).Wait()
    Write-Host "[open] $uri — collecting for $Seconds s"
} catch {
    Write-Host "[connect-fail] $($_.Exception.Message)"
    return
}

$buckets = @{}
$buffer = New-Object byte[] 8192
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and $client.State -eq 'Open') {
    $seg = [ArraySegment[byte]]::new($buffer)
    $recvCts = [System.Threading.CancellationTokenSource]::new(500)
    try {
        $task = $client.ReceiveAsync($seg, $recvCts.Token)
        $task.Wait()
        $result = $task.Result
        if ($result.MessageType -eq 'Close') { break }
        $text = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count)
        try { $obj = $text | ConvertFrom-Json } catch { $obj = $null }
        $t = if ($obj -and $obj.type) { $obj.type } else { '<no-type>' }
        if (-not $buckets.ContainsKey($t)) { $buckets[$t] = [System.Collections.ArrayList]::new() }
        [void]$buckets[$t].Add($text)
    } catch {
        # timeout — keep looping
    }
}

foreach ($k in $buckets.Keys) {
    Write-Host "`n== type=$k count=$($buckets[$k].Count) =="
    Write-Host "first: $($buckets[$k][0])"
    if ($buckets[$k].Count -gt 1) {
        Write-Host "last : $($buckets[$k][-1])"
    }
}

try { $client.CloseAsync('NormalClosure', 'done', [Threading.CancellationToken]::None).Wait(1000) } catch {}
$client.Dispose()
