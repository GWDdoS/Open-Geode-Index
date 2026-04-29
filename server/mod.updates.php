<?php
// mod.updates.php – prox for /v1/mods/updates checking pending ones
header('Content-Type: application/json');

$platform = $_GET['platform'] ?? null;
$gd = $_GET['gd'] ?? null;
$geode = $_GET['geode'] ?? null;
$idsRaw = $_GET['ids'] ?? '';

if (!$platform || !$gd || !$geode || !$idsRaw) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing required parameters', 'payload' => null]);
    exit;
}

$ids = explode(';', $idsRaw);

$upstreamUrl = 'https://api.geode-sdk.org/v1/mods/updates?' . http_build_query([
    'platform' => $platform,
    'gd' => $gd,
    'geode' => $geode,
    'ids' => $idsRaw,
]);

$ch = curl_init($upstreamUrl);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($ch, CURLOPT_TIMEOUT, 10);
$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);

if ($httpCode !== 200) {
    http_response_code($httpCode);
    echo $response ?: json_encode(['error' => 'Upstream error', 'payload' => null]);
    exit;
}

$data = json_decode($response, true);
if (!is_array($data) || !isset($data['payload']['updates'])) {
    echo $response;
    exit;
}

$updatesMap = [];
foreach ($data['payload']['updates'] as $update) {
    if (isset($update['id'])) {
        $updatesMap[$update['id']] = $update;
    }
}

foreach ($ids as $modId) {
    $modUrl = 'https://api.geode-sdk.org/v1/mods/' . urlencode($modId);
    $ch2 = curl_init($modUrl);
    curl_setopt($ch2, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch2, CURLOPT_TIMEOUT, 5);
    $modResp = curl_exec($ch2);
    $modHttp = curl_getinfo($ch2, CURLINFO_HTTP_CODE);
    if ($modHttp !== 200) continue;
    $modData = json_decode($modResp, true);
    if (!isset($modData['payload']['versions'])) continue;
    $bestPending = null;

    foreach ($modData['payload']['versions'] as $ver) {
        if (($ver['status'] ?? '') !== 'pending') continue;
        $gdKey = '';
        if ($platform === 'win') $gdKey = 'win';
        elseif ($platform === 'mac-intel') $gdKey = 'mac-intel';
        elseif ($platform === 'mac-arm') $gdKey = 'mac-arm';
        elseif ($platform === 'ios') $gdKey = 'ios';
        elseif ($platform === 'android32') $gdKey = 'android32';
        elseif ($platform === 'android64') $gdKey = 'android64';
        else continue;
        if (!isset($ver['gd'][$gdKey]) || $ver['gd'][$gdKey] != $gd) continue;
        if (!isset($ver['geode']) || $ver['geode'] != $geode) continue;

        if (!$bestPending || version_compare($ver['version'], $bestPending['version'], '>')) {
            $bestPending = $ver;
        }
    }

    if (!$bestPending) continue;
    $pendingVer = $bestPending['version'];
    $origUpdate = $updatesMap[$modId] ?? null;
    $origVer = $origUpdate['version'] ?? '0.0.0';

    if (!$origUpdate || version_compare($pendingVer, $origVer, '>')) {
        $updatesMap[$modId] = [
            'id' => $modId,
            'version' => $pendingVer,
            'download_link' => $bestPending['download_link'] ?? '',
            'dependencies' => [],
            'incompatibilities' => [],
            'replacement' => null,
        ];
    }
}

$data['payload']['updates'] = array_values($updatesMap);
echo json_encode($data);
