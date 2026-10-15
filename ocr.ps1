Add-Type -AssemblyName System.Drawing
[Windows.Globalization.Language, Windows.Foundation, ContentType = WindowsRuntime] | Out-Null
[Windows.Graphics.Imaging.BitmapDecoder, Windows.Foundation, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType = WindowsRuntime] | Out-Null

$lang = New-Object Windows.Globalization.Language("en-US")
$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage($lang)
if (-not $engine) { $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages() }

function Get-OcrText($imgPath) {
    $file = [System.IO.Path]::GetFullPath($imgPath)
    $asyncOp = [Windows.Storage.StorageFile]::GetFileFromPathAsync($file)
    $storageFile = $asyncOp.GetAwaiter().GetResult()
    $streamOp = $storageFile.OpenAsync([Windows.Storage.FileAccessMode]::Read)
    $stream = $streamOp.GetAwaiter().GetResult()
    $decOp = [Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)
    $dec = $decOp.GetAwaiter().GetResult()
    $bmpOp = $dec.GetSoftwareBitmapAsync()
    $bmp = $bmpOp.GetAwaiter().GetResult()
    $ocrOp = $engine.RecognizeAsync($bmp)
    $res = $ocrOp.GetAwaiter().GetResult()
    return $res.Text
}

foreach ($idx in @(1, 21, 51, 71, 90)) {
    Write-Host "=== TL $idx ==="
    $txt = Get-OcrText "tl_large_${idx}.png"
    Write-Host $txt
}
