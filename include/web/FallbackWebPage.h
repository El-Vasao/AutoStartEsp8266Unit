#pragma once

#include <Arduino.h>
#include <pgmspace.h>

// Минимальная страница “recovery OTA” — используется, когда web-файлы не найдены в LittleFS.
// Цель: дать возможность загрузить OTA-пакет (/upload) и запустить прошивку (/ota/start).

static const char FALLBACK_INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Recovery OTA</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:#0b1220;color:#e6edf3}
    .wrap{max-width:760px;margin:0 auto;padding:24px}
    .card{background:#111a2e;border:1px solid #223055;border-radius:14px;padding:18px;margin:14px 0}
    h1{font-size:20px;margin:0 0 10px}
    p{margin:8px 0;line-height:1.4;color:#c9d4e5}
    .row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
    input[type=file]{flex:1;min-width:240px}
    button{background:#2f81f7;color:#fff;border:0;border-radius:10px;padding:10px 14px;font-weight:600;cursor:pointer}
    button.secondary{background:#2d374d}
    button:disabled{opacity:.6;cursor:not-allowed}
    .small{font-size:12px;color:#9fb0c8}
    .ok{color:#34d399}
    .warn{color:#fbbf24}
    .err{color:#fb7185}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>Recovery OTA</h1>
      <p>Загрузите OTA-пакет и нажмите “Прошить”.</p>
    </div>

    <div class="card">
      <div class="row">
        <input id="file" type="file" accept=".bin,.ota,application/octet-stream">
        <button id="btnUpload">Загрузить</button>
        <button id="btnFlash" class="secondary">Прошить</button>
      </div>
      <p id="status" class="small"></p>
    </div>
  </div>

<script>
  const $ = (id)=>document.getElementById(id);
  const statusEl = $("status");
  function setStatus(msg, cls){
    statusEl.className = "small " + (cls || "");
    statusEl.textContent = msg || "";
  }

  async function post(path){
    const r = await fetch(path, {method:"POST"});
    const t = await r.text();
    return {ok:r.ok, status:r.status, text:t};
  }

  $("btnUpload").onclick = async ()=>{
    const f = $("file").files[0];
    if(!f){ setStatus("Выберите файл OTA-пакета.", "warn"); return; }
    $("btnUpload").disabled = true;
    $("btnFlash").disabled = true;
    try{
      setStatus("Загрузка пакета…");
      const fd = new FormData();
      fd.append("file", f, f.name);
      const r = await fetch("/upload", {method:"POST", body: fd});
      const t = await r.text();
      if(r.ok){
        setStatus("Пакет загружен. Можно прошивать.", "ok");
        $("btnFlash").disabled = false;
      }else{
        setStatus(`Ошибка загрузки (${r.status}): ${t}`, "err");
      }
    }catch(e){
      setStatus(`Ошибка загрузки: ${e}`, "err");
    }finally{
      $("btnUpload").disabled = false;
    }
  };

  $("btnFlash").onclick = async ()=>{
    $("btnFlash").disabled = true;
    try{
      setStatus("Запуск OTA…");
      const r = await post("/ota/start");
      if(r.ok){
        setStatus("OTA запущена. Ожидайте перезагрузку…", "ok");
      }else{
        setStatus(`Не удалось запустить OTA (${r.status}): ${r.text}`, "err");
        $("btnFlash").disabled = false;
      }
    }catch(e){
      setStatus(`Ошибка OTA: ${e}`, "err");
      $("btnFlash").disabled = false;
    }
  };

  // Первичное состояние кнопки “Прошить”
  $("btnFlash").disabled = true;
  setStatus("Выберите файл и нажмите “Загрузить”.");
</script>
</body>
</html>
)HTML";

