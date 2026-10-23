#pragma once
#include <Arduino.h>

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>bot-detector | Control Robótico ESP32-S3</title>
<style>
:root {
  --bg: #0b0f19;
  --card: #151d2e;
  --surface: #0f1523;
  --border: #26334d;
  --text: #f1f5f9;
  --muted: #8b9bb4;
  --accent: #38bdf8;
  --green: #10b981;
  --amber: #f59e0b;
  --red: #ef4444;
  --purple: #8b5cf6;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
body { background: var(--bg); color: var(--text); padding: 16px; max-width: 1120px; margin: 0 auto; line-height: 1.45; }

/* ENCABEZADO MINIMALISTA */
header {
  display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center;
  background: var(--card); border: 1px solid var(--border); border-radius: 14px;
  padding: 14px 20px; margin-bottom: 14px; gap: 12px;
}
.brand h1 { font-size: 1.1rem; font-weight: 800; color: #fff; }
.brand p { font-size: 0.76rem; color: var(--muted); }
.badge-row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
.badge {
  font-size: 0.72rem; font-weight: 700; padding: 5px 12px; border-radius: 999px;
  background: var(--surface); border: 1px solid var(--border);
}
.badge.ok { color: var(--green); border-color: rgba(16,185,129,0.45); background: rgba(16,185,129,0.1); }
.badge.warn { color: var(--amber); border-color: rgba(245,158,11,0.45); background: rgba(245,158,11,0.1); }
.badge.err { color: #fca5a5; border-color: var(--red); background: rgba(239,68,68,0.18); }

/* ALERTA DE SEGURIDAD */
.safety-alert {
  display: none; background: rgba(239, 68, 68, 0.15); border: 1.5px solid var(--red);
  border-radius: 12px; padding: 14px 18px; margin-bottom: 14px;
  justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 12px;
}
.safety-alert.active { display: flex; }

/* BARRA DE ESTADO COMPACTA */
.status-bar {
  background: var(--card); border: 1px solid var(--border); border-radius: 12px;
  padding: 12px 18px; margin-bottom: 16px; display: flex; justify-content: space-between;
  align-items: center; flex-wrap: wrap; gap: 10px;
}
.state-tag {
  background: rgba(56,189,248,0.15); color: var(--accent); border: 1px solid rgba(56,189,248,0.4);
  padding: 4px 10px; border-radius: 6px; font-family: monospace; font-size: 0.76rem; font-weight: 700;
}

/* NAVEGACIÓN PRINCIPAL */
.tabs { display: flex; gap: 10px; margin-bottom: 16px; overflow-x: auto; }
.tab-btn {
  flex: 1; min-width: 160px; padding: 12px 16px; background: var(--card); border: 1px solid var(--border);
  border-radius: 10px; color: var(--muted); font-weight: 700; font-size: 0.85rem;
  cursor: pointer; text-align: center; transition: all 0.15s;
}
.tab-btn.active { background: #0284c7; color: #fff; border-color: var(--accent); }
.tab-pane { display: none; }
.tab-pane.active { display: block; }

/* CONTENEDORES Y TARJETAS CON ESPACIADO LIMPIO */
.grid-2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 16px; }
.studio-layout { display: grid; grid-template-columns: 350px 1fr; gap: 16px; align-items: start; }
@media (max-width: 860px) { .studio-layout { grid-template-columns: 1fr; } }

.card {
  background: var(--card); border: 1px solid var(--border); border-radius: 14px;
  padding: 18px; display: flex; flex-direction: column; gap: 14px;
}
.card-title {
  display: flex; justify-content: space-between; align-items: center;
  border-bottom: 1px solid var(--border); padding-bottom: 10px;
  font-size: 0.88rem; font-weight: 700; color: var(--accent); text-transform: uppercase; letter-spacing: 0.4px;
}

/* SENSORES LIMPIOS */
.sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.sensor-box {
  background: var(--surface); border: 1px solid var(--border); border-radius: 10px;
  padding: 11px 12px; display: flex; align-items: center; gap: 10px;
}
.led { width: 12px; height: 12px; border-radius: 50%; background: #334155; flex-shrink: 0; }
.led.on-green { background: var(--green); box-shadow: 0 0 10px var(--green); }
.led.on-amber { background: var(--amber); box-shadow: 0 0 10px var(--amber); }
.led.on-blue  { background: var(--accent); box-shadow: 0 0 10px var(--accent); }
.sensor-info div { font-size: 0.82rem; font-weight: 700; }
.sensor-info small { font-size: 0.72rem; color: var(--muted); }

.ultra-box { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 12px 14px; }
.ultra-top { display: flex; justify-content: space-between; font-size: 0.82rem; margin-bottom: 8px; }
.ultra-bar-bg { height: 8px; background: #1e293b; border-radius: 4px; overflow: hidden; }
.ultra-bar-fill { height: 100%; background: var(--accent); width: 0%; transition: width 0.15s; }

/* MÉTRICAS */
.stats-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; text-align: center; }
.stat-item { background: var(--surface); padding: 12px 8px; border-radius: 10px; border: 1px solid var(--border); }
.stat-item .val { font-size: 1.4rem; font-weight: 800; color: var(--accent); }
.stat-item .lbl { font-size: 0.72rem; color: var(--muted); margin-top: 2px; }

/* BOTONES */
.btn-row { display: flex; gap: 8px; flex-wrap: wrap; }
button {
  flex: 1; min-width: 96px; padding: 10px 14px; border: 1px solid transparent; border-radius: 8px;
  font-weight: 700; font-size: 0.81rem; cursor: pointer; color: var(--text);
  background: #26334d; transition: all 0.15s;
}
button:hover:not(:disabled) { filter: brightness(1.12); }
button:disabled { opacity: 0.4; cursor: not-allowed; }
button.primary { background: #0284c7; }
button.success { background: #059669; }
button.warn { background: #d97706; }
button.danger { background: var(--red); }
button.purple { background: #7c3aed; }
button.ghost { background: var(--surface); border-color: var(--border); }
button.active-mode { outline: 2px solid #fff; }

/* CONTROLES DE ARTICULACIONES (SLIDER + JOGGING) */
.joint-control {
  background: var(--surface); border: 1px solid var(--border); border-radius: 10px;
  padding: 10px 12px; display: flex; flex-direction: column; gap: 8px;
}
.joint-top { display: flex; justify-content: space-between; align-items: center; font-size: 0.82rem; }
.angle-readout {
  font-family: monospace; font-size: 0.92rem; font-weight: 800; color: var(--accent);
  background: var(--card); padding: 2px 8px; border-radius: 6px; border: 1px solid var(--border);
}
.jog-row { display: flex; align-items: center; gap: 6px; }
.jog-btn {
  flex: 0 0 34px; min-width: 34px; padding: 6px 0; font-size: 0.75rem;
  background: var(--card); border: 1px solid var(--border); border-radius: 6px;
}
input[type=range] { flex: 1; accent-color: var(--accent); height: 6px; cursor: pointer; }

/* SUB-SELECTOR DE MODO EN RUTAS (PARA NO SATURAR LA PANTALLA) */
.sub-nav {
  display: inline-flex; background: var(--surface); border: 1px solid var(--border);
  border-radius: 9px; padding: 4px; gap: 4px; margin-bottom: 4px;
}
.sub-nav button {
  padding: 7px 14px; font-size: 0.78rem; border-radius: 6px; background: transparent;
  color: var(--muted); min-width: auto;
}
.sub-nav button.active { background: var(--card); color: #fff; border-color: var(--border); }

/* PUNTOS DE REFERENCIA LIMPIOS */
.wp-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 12px; }
.wp-card {
  background: var(--surface); border: 1px solid var(--border); border-radius: 10px;
  padding: 14px; display: flex; flex-direction: column; gap: 10px;
}
.wp-card h3 { font-size: 0.84rem; color: #fff; }
.wp-card p { font-size: 0.75rem; color: var(--muted); }
.pose-chips {
  display: flex; gap: 6px; flex-wrap: wrap; font-family: monospace; font-size: 0.75rem;
}
.pose-chip {
  background: var(--card); border: 1px solid var(--border); border-radius: 6px;
  padding: 4px 8px; color: var(--muted);
}
.pose-chip strong { color: var(--accent); }

/* LISTA DE PASOS AVANZADA */
.route-container { display: flex; flex-direction: column; gap: 8px; max-height: 440px; overflow-y: auto; }
.step-card {
  background: var(--surface); border: 1px solid var(--border); border-radius: 10px;
  padding: 12px; display: flex; flex-direction: column; gap: 8px;
}
.step-head { display: flex; justify-content: space-between; align-items: center; }
.step-actions { display: flex; gap: 6px; flex-wrap: wrap; }
.step-actions button { min-width: auto; padding: 6px 10px; font-size: 0.74rem; flex: 1; }
.step-actions button.icon-btn { flex: 0 0 32px; padding: 6px 0; }

.step-editor {
  display: none; background: var(--card); border: 1px solid var(--border);
  border-radius: 8px; padding: 10px; margin-top: 4px; flex-direction: column; gap: 8px;
}
.step-editor.open { display: flex; }
.edit-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.edit-field { display: flex; flex-direction: column; gap: 3px; font-size: 0.71rem; color: var(--muted); }

/* TABLAS Y FORMULARIOS */
.limits-table { width: 100%; border-collapse: collapse; font-size: 0.81rem; }
.limits-table th, .limits-table td { padding: 10px 6px; border-bottom: 1px solid var(--border); text-align: left; }
input[type=number], input[type=text] {
  padding: 7px 9px; background: var(--surface); border: 1px solid var(--border);
  border-radius: 6px; color: var(--text); font-weight: 600; font-size: 0.81rem;
}
.num-sm { width: 70px; text-align: center; }
.form-row {
  display: flex; justify-content: space-between; align-items: center; gap: 12px;
  background: var(--surface); padding: 11px 14px; border-radius: 10px; border: 1px solid var(--border);
  font-size: 0.81rem;
}

#toast {
  position: fixed; bottom: 20px; right: 20px; background: #059669; color: #fff;
  padding: 11px 18px; border-radius: 10px; font-size: 0.84rem; font-weight: 700;
  box-shadow: 0 8px 24px rgba(0,0,0,0.5); opacity: 0; pointer-events: none;
  transition: opacity 0.25s; z-index: 999;
}
#toast.show { opacity: 1; }

.sys-footer {
  margin-top: 16px; font-size: 0.75rem; color: var(--muted);
  display: flex; justify-content: space-between; flex-wrap: wrap; gap: 8px;
  padding: 10px 16px; background: var(--card); border-radius: 10px; border: 1px solid var(--border);
}
</style>
</head>
<body>

<header>
  <div class="brand">
    <h1>bot-detector &bull; Brazo Clasificador ESP32-S3</h1>
    <p>Control Cinemático Suave &bull; Ultrasonido HC-SR04 + Inductivos NPN + Memoria Persistente NVS</p>
  </div>
  <div class="badge-row">
    <span id="wsBadge" class="badge warn">Conectando...</span>
    <span id="nvsBadge" class="badge ok">NVS: Verificando...</span>
    <span id="pcaBadge" class="badge">PCA9685: --</span>
    <span id="modeBadge" class="badge ok">MODO: --</span>
  </div>
</header>

<!-- BANNER DE PARO DE SEGURIDAD -->
<div id="safetyBanner" class="safety-alert">
  <div>
    <strong style="color:#fca5a5;">¡PARO DE SEGURIDAD ACTIVO!</strong>
    <div id="faultReasonTxt" style="font-size:0.8rem;margin-top:2px;">El sistema cortó el PWM por seguridad.</div>
  </div>
  <button class="success" style="flex:0;min-width:200px;" onclick="sendCmd({cmd:'ack_fault'});showToast('Seguridad restablecida')">Desbloquear Sistema</button>
</div>

<!-- BARRA DE ESTADO COMPACTA -->
<div class="status-bar">
  <div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap;">
    <span id="fsmStateBadge" class="state-tag">IDLE</span>
    <strong id="statusText" style="font-size:0.88rem;">Inicializando...</strong>
  </div>
  <div style="font-size:0.79rem;color:var(--muted);">Última acción: <strong id="lastResultTxt" style="color:var(--accent)">--</strong></div>
</div>

<!-- PESTAÑAS PRINCIPALES -->
<div class="tabs">
  <button class="tab-btn active" onclick="switchTab(0)">1. Operación y Monitoreo</button>
  <button class="tab-btn" onclick="switchTab(1)">2. Programar Rutas (NVS)</button>
  <button class="tab-btn" onclick="switchTab(2)">3. Límites y Calibración</button>
</div>

<!-- ================= PESTAÑA 1: OPERACIÓN Y MONITOREO ================= -->
<div id="tab0" class="tab-pane active">
  <div class="grid-2">
    <div class="card">
      <div class="card-title">
        <span>Sensores en Tiempo Real</span>
        <small style="color:var(--muted);font-size:0.72rem;text-transform:none;">HC-SR04 + Inductivos</small>
      </div>

      <div class="ultra-box">
        <div class="ultra-top">
          <span><strong>Distancia HC-SR04:</strong> <span id="ultraCmTxt" style="color:var(--accent);font-weight:700;">-- cm</span></span>
          <span>Umbral Pieza: <strong id="ultraThrTxt">12.0 cm</strong></span>
        </div>
        <div class="ultra-bar-bg"><div id="ultraBar" class="ultra-bar-fill"></div></div>
      </div>

      <div class="sensor-grid">
        <div class="sensor-box">
          <div id="ledPart" class="led"></div>
          <div class="sensor-info">
            <div>Presencia (HC-SR04)</div>
            <small id="txtPart">Zona Libre</small>
          </div>
        </div>
        <div class="sensor-box">
          <div id="ledInd" class="led"></div>
          <div class="sensor-info">
            <div>Tipo de Material</div>
            <small id="txtInd">No Metálico</small>
          </div>
        </div>
        <div class="sensor-box">
          <div id="ledLimL" class="led"></div>
          <div class="sensor-info">
            <div>Tope Base Izquierda</div>
            <small>GPIO 5 &bull; <span id="txtLimL">Libre</span></small>
          </div>
        </div>
        <div class="sensor-box">
          <div id="ledLimR" class="led"></div>
          <div class="sensor-info">
            <div>Tope Base Derecha</div>
            <small>GPIO 6 &bull; <span id="txtLimR">Libre</span></small>
          </div>
        </div>
      </div>

      <div class="stats-row">
        <div class="stat-item">
          <div id="cntMetal" class="val">0</div>
          <div class="lbl">Metales</div>
        </div>
        <div class="stat-item">
          <div id="cntNonMetal" class="val">0</div>
          <div class="lbl">No Metales</div>
        </div>
        <div class="stat-item">
          <div id="cntFault" class="val" style="color:var(--amber)">0</div>
          <div class="lbl">Paros Seguridad</div>
        </div>
      </div>

      <div class="btn-row">
        <button id="btnAuto" class="success" onclick="sendCmd({cmd:'set_mode',mode:'auto'});showToast('Modo Automático Activo')">MODO AUTOMÁTICO</button>
        <button id="btnManual" class="primary" onclick="sendCmd({cmd:'set_mode',mode:'manual'});showToast('Modo Manual Activo')">MODO MANUAL</button>
      </div>
      <div class="btn-row">
        <button class="warn" onclick="sendCmd({cmd:'homing'});showToast('Ejecutando Homing...')">Homing Base</button>
        <button class="ghost" onclick="sendCmd({cmd:'reset_counters'});showToast('Contadores reiniciados')">Reiniciar Conteo</button>
        <button class="danger" onclick="sendCmd({cmd:'estop'})">PARO DE EMERGENCIA</button>
      </div>
    </div>

    <!-- CONTROL MANUAL -->
    <div class="card">
      <div class="card-title">
        <span>Control Manual de Servomotores</span>
        <small id="manualLockTag" style="font-size:0.72rem;text-transform:none;"></small>
      </div>

      <div id="mainSlidersMount"></div>

      <div class="joint-control">
        <div class="joint-top">
          <strong>Velocidad de Movimiento</strong>
          <span class="angle-readout"><span id="valSpd">70</span> &deg;/s</span>
        </div>
        <input type="range" id="slSpd" min="15" max="180" value="70" onchange="sendCmd({cmd:'set_speed',speed:parseFloat(this.value)})" oninput="document.getElementById('valSpd').innerText=this.value">
      </div>

      <div class="btn-row">
        <button id="btnGoHome" class="ghost" onclick="sendCmd({cmd:'goto_home'});showToast('Yendo a HOME...')">Ir a HOME</button>
        <button id="btnSaveHome" class="primary" onclick="sendCmd({cmd:'save_home'});showToast('Guardado en Memoria Flash NVS: Posición HOME')">Guardar Actual como HOME</button>
      </div>
    </div>
  </div>
</div>

<!-- ================= PESTAÑA 2: PROGRAMAR RUTAS (GUARDADO PERSISTENTE EN FLASH NVS) ================= -->
<div id="tab1" class="tab-pane">
  <div class="studio-layout">
    <!-- CONSOLA IZQUIERDA PARA MOVER EL BRAZO -->
    <div class="card" style="position:sticky;top:12px;">
      <div class="card-title">
        <span>Mover Brazo (Teach-In)</span>
      </div>
      <div id="studioSlidersMount"></div>
      <div class="btn-row">
        <button class="ghost" onclick="sendCmd({cmd:'goto_home'});showToast('Moviendo a HOME...')">Ir a HOME</button>
      </div>
    </div>

    <!-- PANEL DERECHO CON SUB-MODO LIMPIO -->
    <div class="card">
      <div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;border-bottom:1px solid var(--border);padding-bottom:10px;">
        <div class="sub-nav">
          <button id="subBtnQuick" class="active" onclick="switchStudioMode('quick')">Modo Rápido (4 Puntos de Referencia)</button>
          <button id="subBtnSteps" onclick="switchStudioMode('steps')">Editar Pasos Individuales</button>
        </div>
        <div style="display:flex;gap:6px;">
          <button class="primary" style="flex:0;min-width:auto;padding:7px 12px;" onclick="sendCmd({cmd:'test_route',isMetal:true});showToast('Probando Ruta Metal...')">Probar Metal</button>
          <button class="purple" style="flex:0;min-width:auto;padding:7px 12px;" onclick="sendCmd({cmd:'test_route',isMetal:false});showToast('Probando Ruta No-Metal...')">Probar No-Metal</button>
        </div>
      </div>

      <!-- SUB-VISTA A: ASISTENTE RÁPIDO DE 4 PUNTOS DE REFERENCIA -->
      <div id="studioQuickView">
        <p style="font-size:0.8rem;color:var(--muted);margin-bottom:12px;">
          Mueve el brazo con los controles de la izquierda y pulsa <strong>"Guardar en Flash NVS"</strong> en cada tarjeta. El ESP32-S3 guarda los puntos en memoria permanente (no se borran al desconectar la energía) y arma la trayectoria completa automáticamente:
        </p>

        <div class="wp-grid">
          <div class="wp-card">
            <h3>1. Apertura y Cierre de Pinza + Altura</h3>
            <p>Ajusta CH3 Pinza para abrir/cerrar y la altura segura de giro:</p>
            <div class="pose-chips">
              <span class="pose-chip">Abierta: <strong id="wpGripOpen">65&deg;</strong></span>
              <span class="pose-chip">Cerrada: <strong id="wpGripClose">95&deg;</strong></span>
              <span class="pose-chip">Altura Giro: <strong id="wpSafeEl">115&deg;</strong></span>
            </div>
            <div class="btn-row">
              <button class="ghost" onclick="captureWp('grip_open','Pinza ABIERTA guardada en Flash NVS')">Fijar Abierta</button>
              <button class="ghost" onclick="captureWp('grip_close','Pinza CERRADA guardada en Flash NVS')">Fijar Cerrada</button>
            </div>
            <div class="btn-row">
              <button class="primary" onclick="sendCmd({cmd:'save_home'});showToast('Altura HOME guardada en Flash NVS')">Guardar Altura Segura / HOME</button>
            </div>
          </div>

          <div class="wp-card">
            <h3>2. Punto de Recogida (Sensores)</h3>
            <p>Coloca el brazo abajo rodeando la pieza frente a los sensores:</p>
            <div id="wpPickPose" class="pose-chips"></div>
            <div class="btn-row">
              <button class="ghost" onclick="sendCmd({cmd:'goto_waypoint',which:'pick'})">Ir a Punto</button>
              <button class="success" onclick="captureWp('pick','Punto de Recogida guardado en Flash NVS')">Guardar en Flash NVS</button>
            </div>
          </div>

          <div class="wp-card">
            <h3>3. Destino Pieza METÁLICA</h3>
            <p>Posición donde soltará las piezas cuando detecte metal:</p>
            <div id="wpDropMetalPose" class="pose-chips"></div>
            <div class="btn-row">
              <button class="ghost" onclick="sendCmd({cmd:'goto_waypoint',which:'drop_metal'})">Ir a Punto</button>
              <button class="success" onclick="captureWp('drop_metal','Destino Metal guardado en Flash NVS')">Guardar en Flash NVS</button>
            </div>
          </div>

          <div class="wp-card">
            <h3>4. Destino Pieza NO METÁLICA</h3>
            <p>Posición donde soltará objetos detectados solo por ultrasonido:</p>
            <div id="wpDropNonMetalPose" class="pose-chips"></div>
            <div class="btn-row">
              <button class="ghost" onclick="sendCmd({cmd:'goto_waypoint',which:'drop_nonmetal'})">Ir a Punto</button>
              <button class="purple" onclick="captureWp('drop_nonmetal','Destino No-Metal guardado en Flash NVS')">Guardar en Flash NVS</button>
            </div>
          </div>
        </div>
      </div>

      <!-- SUB-VISTA B: EDITOR PASO POR PASO DETALLADO -->
      <div id="studioStepsView" style="display:none;">
        <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:8px;">
          <div class="sub-nav">
            <button id="btnViewMetal" class="active" onclick="switchRouteList(true)">Ver Pasos Ruta METAL</button>
            <button id="btnViewNonMetal" onclick="switchRouteList(false)">Ver Pasos Ruta NO-METAL</button>
          </div>
        </div>

        <!-- CONTENEDOR RUTA METAL -->
        <div id="metalStepsBox">
          <div id="metalRouteList" class="route-container"></div>
          <div class="form-row" style="margin-top:10px;">
            <input type="text" id="metalStepLbl" placeholder="Nombre del paso extra..." style="flex:1;">
            <span>Espera (ms):</span>
            <input type="number" class="num-sm" id="metalStepDwell" value="400" min="100" max="5000" step="50">
            <button class="success" style="flex:0;min-width:160px;" onclick="addRouteStep(true)">+ Añadir Paso a NVS</button>
          </div>
        </div>

        <!-- CONTENEDOR RUTA NO METAL -->
        <div id="nonMetalStepsBox" style="display:none;">
          <div id="nonMetalRouteList" class="route-container"></div>
          <div class="form-row" style="margin-top:10px;">
            <input type="text" id="nonMetalStepLbl" placeholder="Nombre del paso extra..." style="flex:1;">
            <span>Espera (ms):</span>
            <input type="number" class="num-sm" id="nonMetalStepDwell" value="400" min="100" max="5000" step="50">
            <button class="purple" style="flex:0;min-width:160px;" onclick="addRouteStep(false)">+ Añadir Paso a NVS</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- ================= PESTAÑA 3: LÍMITES Y CALIBRACIÓN ================= -->
<div id="tab2" class="tab-pane">
  <div class="grid-2">
    <div class="card">
      <div class="card-title">
        <span>Límites de Seguridad (15&deg; a 165&deg;)</span>
      </div>
      <table class="limits-table">
        <thead>
          <tr><th>Articulación</th><th>Mín (&deg;)</th><th>Máx (&deg;)</th><th>Actual / Captura</th></tr>
        </thead>
        <tbody>
          <tr>
            <td><strong>CH0: Base</strong></td>
            <td><input type="number" class="num-sm" id="minJ0" min="0" max="170" value="15"></td>
            <td><input type="number" class="num-sm" id="maxJ0" min="10" max="180" value="165"></td>
            <td><strong id="curLim0" style="color:var(--accent)">90&deg;</strong> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(0,true)">=Mín</button> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(0,false)">=Máx</button></td>
          </tr>
          <tr>
            <td><strong>CH1: Extensión</strong></td>
            <td><input type="number" class="num-sm" id="minJ1" min="0" max="170" value="15"></td>
            <td><input type="number" class="num-sm" id="maxJ1" min="10" max="180" value="165"></td>
            <td><strong id="curLim1" style="color:var(--accent)">70&deg;</strong> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(1,true)">=Mín</button> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(1,false)">=Máx</button></td>
          </tr>
          <tr>
            <td><strong>CH2: Elevación</strong></td>
            <td><input type="number" class="num-sm" id="minJ2" min="0" max="170" value="15"></td>
            <td><input type="number" class="num-sm" id="maxJ2" min="10" max="180" value="165"></td>
            <td><strong id="curLim2" style="color:var(--accent)">115&deg;</strong> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(2,true)">=Mín</button> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(2,false)">=Máx</button></td>
          </tr>
          <tr>
            <td><strong>CH3: Pinza</strong></td>
            <td><input type="number" class="num-sm" id="minJ3" min="0" max="170" value="15"></td>
            <td><input type="number" class="num-sm" id="maxJ3" min="10" max="180" value="165"></td>
            <td><strong id="curLim3" style="color:var(--accent)">75&deg;</strong> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(3,true)">=Mín</button> <button class="ghost" style="padding:4px 8px;min-width:auto" onclick="setLimitFromCurrent(3,false)">=Máx</button></td>
          </tr>
        </tbody>
      </table>
      <div class="btn-row">
        <button class="success" onclick="saveAllLimits()">Guardar Límites en Flash NVS</button>
        <button class="ghost" onclick="resetLimits15_165()">Restablecer 15&deg; - 165&deg;</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">
        <span>Parámetros de Sensores y Tiempos</span>
      </div>
      <div class="form-row">
        <span>Umbral de distancia HC-SR04 para detectar pieza (cm):</span>
        <input type="number" class="num-sm" id="cfgDistCm" step="0.5" min="2" max="80" value="12.0">
      </div>
      <div class="form-row">
        <span>Tiempo de confirmación de material (ms):</span>
        <input type="number" class="num-sm" id="cfgVerifyMs" step="50" min="150" max="5000" value="700">
      </div>
      <div class="form-row">
        <span>Tiempo máx. de movimiento antes de declarar atasco (ms):</span>
        <input type="number" class="num-sm" id="cfgTimeoutMs" step="100" min="1000" max="15000" value="4500">
      </div>
      <div class="form-row">
        <label for="cfgIndActiveLow" style="cursor:pointer;flex:1;">Sensores Inductivos Activos en LOW (0V):</label>
        <input type="checkbox" id="cfgIndActiveLow" checked style="width:20px;height:20px;">
      </div>
      <div class="btn-row">
        <button class="primary" onclick="saveCalibParams()">Guardar Calibración en Flash NVS</button>
      </div>
      <div class="btn-row">
        <button class="danger" onclick="if(confirm('¿Borrar la memoria NVS y restaurar valores de fábrica?')){sendCmd({cmd:'factory_reset'});showToast('Memoria NVS restaurada de fábrica');}">Borrar NVS y Restaurar de Fábrica</button>
      </div>
    </div>
  </div>
</div>

<div id="toast">Guardado en Memoria Flash NVS</div>

<div class="sys-footer">
  <span>Red Wi-Fi: <strong>bot-detector</strong> &bull; IP: <strong>192.168.4.1</strong> &bull; 4 Servos con Auto-Relax Térmico</span>
  <span>Activo: <strong id="uptimeTxt">0s</strong> &bull; RAM: <strong id="heapTxt">-- KB</strong></span>
</div>

<script>
const JOINT_NAMES = [
  "CH0: Base (Izq/Der)",
  "CH1: Extensión",
  "CH2: Elevación",
  "CH3: Pinza"
];

let ws;
let isDragging = false;
let lastSendMs = 0;
let currentAngles = [90, 70, 115, 75];
let jointLimits = { min: [15, 15, 15, 15], max: [165, 165, 165, 165] };

function buildJointControls(containerId, prefix) {
  const wrap = document.getElementById(containerId);
  wrap.innerHTML = JOINT_NAMES.map((name, i) => `
    <div class="joint-control" style="margin-bottom:8px;">
      <div class="joint-top">
        <strong>${name} <small style="color:var(--muted);font-weight:normal;" id="${prefix}LimTag${i}">(15°-165°)</small></strong>
        <span class="angle-readout"><span id="${prefix}Val${i}">90</span>&deg;</span>
      </div>
      <div class="jog-row">
        <button class="jog-btn" onclick="jogJoint(${i}, -5)">-5&deg;</button>
        <button class="jog-btn" onclick="jogJoint(${i}, -1)">-1&deg;</button>
        <input type="range" id="${prefix}Sl${i}" min="15" max="165" value="90" oninput="onJointSlide(${i}, this.value)">
        <button class="jog-btn" onclick="jogJoint(${i}, 1)">+1&deg;</button>
        <button class="jog-btn" onclick="jogJoint(${i}, 5)">+5&deg;</button>
      </div>
    </div>
  `).join('');
}

buildJointControls('mainSlidersMount', 'm');
buildJointControls('studioSlidersMount', 's');

function showToast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2400);
}

function switchTab(idx) {
  document.querySelectorAll('.tab-btn').forEach((b, i) => b.classList.toggle('active', i === idx));
  document.querySelectorAll('.tab-pane').forEach((p, i) => p.classList.toggle('active', i === idx));
}

function switchStudioMode(mode) {
  const isQuick = (mode === 'quick');
  document.getElementById('subBtnQuick').classList.toggle('active', isQuick);
  document.getElementById('subBtnSteps').classList.toggle('active', !isQuick);
  document.getElementById('studioQuickView').style.display = isQuick ? 'block' : 'none';
  document.getElementById('studioStepsView').style.display = isQuick ? 'none' : 'block';
}

function switchRouteList(isMetal) {
  document.getElementById('btnViewMetal').classList.toggle('active', isMetal);
  document.getElementById('btnViewNonMetal').classList.toggle('active', !isMetal);
  document.getElementById('metalStepsBox').style.display    = isMetal ? 'block' : 'none';
  document.getElementById('nonMetalStepsBox').style.display = isMetal ? 'none' : 'block';
}

function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}/ws`);

  ws.onopen = () => {
    const b = document.getElementById('wsBadge');
    b.textContent = 'Conectado';
    b.className = 'badge ok';
    fetch('/api/config').then(r => r.json()).then(applyConfigData).catch(() => {});
  };

  ws.onclose = () => {
    const b = document.getElementById('wsBadge');
    b.textContent = 'Reconectando...';
    b.className = 'badge warn';
    setTimeout(connectWS, 1500);
  };

  ws.onmessage = (ev) => {
    try {
      const d = JSON.parse(ev.data);
      if (d.type === 'config') {
        applyConfigData(d);
      } else {
        updateTelemetry(d);
      }
    } catch (e) {}
  };
}

function sendCmd(payload) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(payload));
  }
}

function syncJointUI(joint, val) {
  const rounded = Math.round(val);
  currentAngles[joint] = rounded;
  ['m', 's'].forEach(pref => {
    const vEl = document.getElementById(`${pref}Val${joint}`);
    const sEl = document.getElementById(`${pref}Sl${joint}`);
    if (vEl) vEl.textContent = rounded;
    if (sEl) sEl.value = rounded;
  });
}

function onJointSlide(joint, val) {
  syncJointUI(joint, val);
  const now = Date.now();
  if (now - lastSendMs > 40) {
    sendCmd({ cmd: 'move_joint', joint: joint, angle: parseFloat(val) });
    lastSendMs = now;
  }
}

function jogJoint(joint, delta) {
  const mn = jointLimits.min[joint] || 15;
  const mx = jointLimits.max[joint] || 165;
  const nextVal = Math.max(mn, Math.min(mx, Math.round(currentAngles[joint]) + delta));
  syncJointUI(joint, nextVal);
  sendCmd({ cmd: 'move_joint', joint: joint, angle: nextVal });
}

function setLimitFromCurrent(joint, isMin) {
  const val = Math.round(currentAngles[joint]);
  document.getElementById((isMin ? 'minJ' : 'maxJ') + joint).value = val;
  showToast(`Ángulo ${val}° copiado`);
}

function saveAllLimits() {
  const mins = [], maxs = [];
  for (let i = 0; i < 4; i++) {
    mins.push(parseFloat(document.getElementById('minJ' + i).value));
    maxs.push(parseFloat(document.getElementById('maxJ' + i).value));
  }
  sendCmd({ cmd: 'save_limits', min: mins, max: maxs });
  showToast('Límites guardados permanentemente en Flash NVS');
}

function resetLimits15_165() {
  for (let i = 0; i < 4; i++) {
    document.getElementById('minJ' + i).value = 15;
    document.getElementById('maxJ' + i).value = 165;
  }
  saveAllLimits();
}

function saveCalibParams() {
  sendCmd({
    cmd: 'save_calib',
    distCm: parseFloat(document.getElementById('cfgDistCm').value),
    verifyMs: parseInt(document.getElementById('cfgVerifyMs').value),
    timeoutMs: parseInt(document.getElementById('cfgTimeoutMs').value),
    indActiveLow: document.getElementById('cfgIndActiveLow').checked
  });
  showToast('Calibración guardada en Flash NVS');
}

function captureWp(which, msg) {
  sendCmd({ cmd: 'capture_waypoint', which: which });
  showToast(msg);
}

function renderWpPose(elId, arr) {
  const el = document.getElementById(elId);
  if (!el || !arr) return;
  el.innerHTML = `
    <span class="pose-chip">B: <strong>${Math.round(arr[0])}&deg;</strong></span>
    <span class="pose-chip">Ex: <strong>${Math.round(arr[1])}&deg;</strong></span>
    <span class="pose-chip">El: <strong>${Math.round(arr[2])}&deg;</strong></span>
    <span class="pose-chip">P: <strong>${Math.round(arr[3])}&deg;</strong></span>
  `;
}

function addRouteStep(isMetal) {
  const lblInput = document.getElementById(isMetal ? 'metalStepLbl' : 'nonMetalStepLbl');
  const dwInput  = document.getElementById(isMetal ? 'metalStepDwell' : 'nonMetalStepDwell');
  const label    = lblInput.value.trim() || (isMetal ? 'Paso Metal' : 'Paso No-Metal');
  const dwell    = parseInt(dwInput.value) || 400;
  sendCmd({ cmd: 'add_route_step', isMetal: isMetal, label: label, dwellMs: dwell });
  lblInput.value = '';
  showToast(`Paso "${label}" guardado en Flash NVS`);
}

function toggleStepEditor(prefix, idx) {
  const el = document.getElementById(`ed_${prefix}_${idx}`);
  if (el) el.classList.toggle('open');
}

function saveStepFullEdit(isMetal, prefix, idx) {
  const lbl   = document.getElementById(`edLbl_${prefix}_${idx}`).value.trim() || 'Paso';
  const dwell = parseInt(document.getElementById(`edDw_${prefix}_${idx}`).value) || 400;
  const b     = parseFloat(document.getElementById(`edB_${prefix}_${idx}`).value);
  const ex    = parseFloat(document.getElementById(`edEx_${prefix}_${idx}`).value);
  const el    = parseFloat(document.getElementById(`edEl_${prefix}_${idx}`).value);
  const gr    = parseFloat(document.getElementById(`edGr_${prefix}_${idx}`).value);

  sendCmd({
    cmd: 'edit_step_full',
    isMetal: isMetal,
    index: idx,
    label: lbl,
    dwellMs: dwell,
    b: b, ex: ex, el: el, gr: gr
  });
  showToast(`Paso #${idx + 1} guardado en Flash NVS`);
}

function captureCurrentIntoStep(isMetal, idx, dwell) {
  sendCmd({ cmd: 'update_step', isMetal: isMetal, index: idx, dwellMs: dwell });
  showToast(`Postura actual guardada en Paso #${idx + 1} (NVS)`);
}

function renderRouteList(containerId, steps, isMetal) {
  const el = document.getElementById(containerId);
  const prefix = isMetal ? 'met' : 'nomet';

  if (!steps || steps.length === 0) {
    el.innerHTML = '<div style="color:var(--muted);font-size:0.8rem;padding:12px;text-align:center;">Sin pasos. Usa el Modo Rápido o añade un paso abajo.</div>';
    return;
  }

  el.innerHTML = steps.map((s, idx) => {
    const b   = Math.round(s.pose[0]);
    const ex  = Math.round(s.pose[1]);
    const elA = Math.round(s.pose[2]);
    const gr  = Math.round(s.pose[3]);
    return `
      <div class="step-card">
        <div class="step-head">
          <div>
            <strong style="color:var(--accent);font-size:0.82rem;">#${idx + 1} ${s.label}</strong>
            <small style="color:var(--muted);margin-left:6px;">(${s.dwell} ms)</small>
          </div>
          <div class="pose-chips">
            <span class="pose-chip">B:<strong>${b}&deg;</strong></span>
            <span class="pose-chip">Ex:<strong>${ex}&deg;</strong></span>
            <span class="pose-chip">El:<strong>${elA}&deg;</strong></span>
            <span class="pose-chip">P:<strong>${gr}&deg;</strong></span>
          </div>
        </div>

        <div class="step-actions">
          <button class="ghost" onclick="sendCmd({cmd:'goto_step',isMetal:${isMetal},index:${idx}})">Ir / Probar</button>
          <button class="primary" onclick="captureCurrentIntoStep(${isMetal},${idx},${s.dwell})">Capturar Actual Aquí</button>
          <button class="ghost" onclick="toggleStepEditor('${prefix}',${idx})">Editar Ángulos</button>
          <button class="ghost icon-btn" ${idx === 0 ? 'disabled' : ''} onclick="sendCmd({cmd:'move_step_order',isMetal:${isMetal},index:${idx},dir:-1})">&uarr;</button>
          <button class="ghost icon-btn" ${idx === steps.length - 1 ? 'disabled' : ''} onclick="sendCmd({cmd:'move_step_order',isMetal:${isMetal},index:${idx},dir:1})">&darr;</button>
          <button class="danger icon-btn" onclick="if(confirm('¿Eliminar Paso #${idx+1}?')) sendCmd({cmd:'delete_step',isMetal:${isMetal},index:${idx}})">&times;</button>
        </div>

        <div id="ed_${prefix}_${idx}" class="step-editor">
          <div style="display:flex;gap:8px;">
            <div class="edit-field" style="flex:2;"><span>Nombre:</span><input type="text" id="edLbl_${prefix}_${idx}" value="${s.label}"></div>
            <div class="edit-field" style="flex:1;"><span>Espera (ms):</span><input type="number" id="edDw_${prefix}_${idx}" value="${s.dwell}" min="100" max="10000" step="50"></div>
          </div>
          <div class="edit-grid">
            <div class="edit-field"><span>Base (&deg;)</span><input type="number" id="edB_${prefix}_${idx}" value="${b}" min="0" max="180"></div>
            <div class="edit-field"><span>Ext (&deg;)</span><input type="number" id="edEx_${prefix}_${idx}" value="${ex}" min="0" max="180"></div>
            <div class="edit-field"><span>Elev (&deg;)</span><input type="number" id="edEl_${prefix}_${idx}" value="${elA}" min="0" max="180"></div>
            <div class="edit-field"><span>Pinza (&deg;)</span><input type="number" id="edGr_${prefix}_${idx}" value="${gr}" min="0" max="180"></div>
          </div>
          <button class="success" onclick="saveStepFullEdit(${isMetal},'${prefix}',${idx})">Guardar Cambios en Flash NVS</button>
        </div>
      </div>
    `;
  }).join('');
}

function applyConfigData(cfg) {
  const nvsB = document.getElementById('nvsBadge');
  if (cfg.nvsSaved) {
    nvsB.textContent = `NVS: Guardado (#${cfg.nvsCount || 1})`;
    nvsB.className   = 'badge ok';
  } else {
    nvsB.textContent = 'NVS: Valores por Defecto';
    nvsB.className   = 'badge warn';
  }

  if (cfg.limits) {
    jointLimits = cfg.limits;
    for (let i = 0; i < 4; i++) {
      const mn = Math.round(cfg.limits.min[i]);
      const mx = Math.round(cfg.limits.max[i]);
      document.getElementById('minJ' + i).value = mn;
      document.getElementById('maxJ' + i).value = mx;

      ['m', 's'].forEach(pref => {
        const tag = document.getElementById(`${pref}LimTag${i}`);
        const sl  = document.getElementById(`${pref}Sl${i}`);
        if (tag) tag.textContent = `(${mn}°-${mx}°)`;
        if (sl)  { sl.min = mn; sl.max = mx; }
      });
    }
  }
  if (cfg.calib) {
    document.getElementById('cfgDistCm').value          = cfg.calib.distCm;
    document.getElementById('cfgVerifyMs').value        = cfg.calib.verifyMs;
    document.getElementById('cfgTimeoutMs').value       = cfg.calib.timeoutMs;
    document.getElementById('cfgIndActiveLow').checked  = (cfg.calib.indActiveLow !== false);
    document.getElementById('ultraThrTxt').textContent  = cfg.calib.distCm.toFixed(1) + ' cm';
  }
  if (cfg.waypoints) {
    document.getElementById('wpGripOpen').innerHTML  = Math.round(cfg.waypoints.gripOpen) + '&deg;';
    document.getElementById('wpGripClose').innerHTML = Math.round(cfg.waypoints.gripClose) + '&deg;';
    document.getElementById('wpSafeEl').innerHTML    = Math.round(cfg.waypoints.safeEl) + '&deg;';
    renderWpPose('wpPickPose',        cfg.waypoints.pick);
    renderWpPose('wpDropMetalPose',   cfg.waypoints.dropMetal);
    renderWpPose('wpDropNonMetalPose',cfg.waypoints.dropNonMetal);
  }
  if (cfg.metalRoute) {
    renderRouteList('metalRouteList', cfg.metalRoute.steps, true);
  }
  if (cfg.nonMetalRoute) {
    renderRouteList('nonMetalRouteList', cfg.nonMetalRoute.steps, false);
  }
}

function setLed(id, txtId, active, cls, onTxt, offTxt) {
  document.getElementById(id).className = active ? ('led ' + cls) : 'led';
  document.getElementById(txtId).textContent = active ? onTxt : offTxt;
}

function updateTelemetry(d) {
  document.getElementById('fsmStateBadge').textContent = d.stateName;
  document.getElementById('statusText').textContent    = d.status;
  document.getElementById('lastResultTxt').textContent = d.lastResult;

  const safetyBanner = document.getElementById('safetyBanner');
  safetyBanner.classList.toggle('active', d.safetyActive);
  if (d.safetyActive) {
    document.getElementById('faultReasonTxt').textContent = d.status;
  }

  const pcaB = document.getElementById('pcaBadge');
  pcaB.textContent = d.pcaOnline ? 'PCA9685: En Línea' : 'PCA9685: Sin conexión';
  pcaB.className   = d.pcaOnline ? 'badge ok' : 'badge err';

  const modeB = document.getElementById('modeBadge');
  modeB.textContent = 'MODO: ' + d.mode;
  modeB.className   = d.safetyActive ? 'badge err' : (d.mode === 'AUTO' ? 'badge ok' : 'badge warn');

  document.getElementById('btnAuto').classList.toggle('active-mode', d.mode === 'AUTO');
  document.getElementById('btnManual').classList.toggle('active-mode', d.mode === 'MANUAL');

  const safetyBlocked = d.safetyActive;
  document.getElementById('manualLockTag').textContent = safetyBlocked
    ? 'Bloqueado por Seguridad'
    : (d.mode === 'MANUAL' ? 'Control Manual Activo' : 'Mover slider cambia a Manual');

  for (let i = 0; i < 4; i++) {
    ['m', 's'].forEach(pref => {
      const sl = document.getElementById(`${pref}Sl${i}`);
      if (sl) sl.disabled = safetyBlocked;
    });
  }
  document.querySelectorAll('.jog-btn').forEach(b => b.disabled = safetyBlocked);
  document.getElementById('btnGoHome').disabled   = safetyBlocked;
  document.getElementById('btnSaveHome').disabled = safetyBlocked;

  const distTxt = (d.distCm > 190) ? 'Fuera de rango' : (d.distCm.toFixed(1) + ' cm');
  document.getElementById('ultraCmTxt').textContent = distTxt;
  const pct = (d.distCm > 190) ? 0 : Math.max(0, Math.min(100, (1 - (d.distCm / 40.0)) * 100));
  const bar = document.getElementById('ultraBar');
  bar.style.width = pct + '%';
  bar.style.background = d.partPresent ? 'var(--green)' : 'var(--accent)';

  setLed('ledPart', 'txtPart', d.partPresent, 'on-blue', '¡Pieza Detectada!', 'Zona Libre');
  const indDetail = d.metal
    ? `¡METAL! (${d.ind1 ? 'S1 ' : ''}${d.ind2 ? 'S2' : ''})`
    : 'No Metálico';
  setLed('ledInd',  'txtInd',  d.metal,       'on-green', indDetail, indDetail);
  setLed('ledLimL', 'txtLimL', d.limL,        'on-amber', '¡Tope Activo!', 'Libre');
  setLed('ledLimR', 'txtLimR', d.limR,        'on-amber', '¡Tope Activo!', 'Libre');

  document.getElementById('cntMetal').textContent    = d.metalCnt;
  document.getElementById('cntNonMetal').textContent = d.nonMetalCnt;
  document.getElementById('cntFault').textContent    = d.faultCnt;

  if (d.angles) {
    for (let i = 0; i < 4; i++) {
      const ang = Math.round(d.angles[i]);
      document.getElementById('curLim' + i).textContent = ang + '°';
      if (!isDragging) {
        syncJointUI(i, ang);
      }
    }
  }

  document.getElementById('uptimeTxt').textContent = d.uptime + 's';
  document.getElementById('heapTxt').textContent   = Math.round(d.heap / 1024) + ' KB';
}

document.addEventListener('pointerdown', (e) => { if (e.target.type === 'range') isDragging = true; });
document.addEventListener('pointerup',   () => { isDragging = false; });
document.addEventListener('touchend',    () => { isDragging = false; });

window.addEventListener('load', connectWS);
</script>
</body>
</html>
)rawliteral";
