/* creamlinux web UI - vanilla JS, no frameworks */
"use strict";

let games = [];
let logTimer = null;

const $ = (sel) => document.querySelector(sel);

async function api(path, opts = {}) {
  const resp = await fetch(path, {
    method: opts.method || "GET",
    headers: opts.body ? {"Content-Type": "application/json"} : {},
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const data = await resp.json().catch(() => ({}));
  if (!resp.ok) throw new Error(data.error || `HTTP ${resp.status}`);
  return data;
}

/* ------------------------------------------------------------ rendering - */

function badge(type) {
  const map = {native: "native", proton: "proton", creamlinux: "creamlinux",
               smokeapi: "smokeapi"};
  const cls = map[type] || "none";
  const label = type || "—";
  return `<span class="badge ${cls}">${label}</span>`;
}

const DLC_HINTS = {
  ok: "Файлы DLC найдены",
  partial: "Файлы DLC есть, но мало (проверь папку dlc)",
  none: "Файлы DLC не найдены — скачай папку dlc отдельно!",
  unknown: "Не проверяется (Proton)",
};

function dlcBadge(status) {
  const labels = {ok: "✅ есть", partial: "⚠️ частично", none: "❌ нет",
                  unknown: "—"};
  const hint = DLC_HINTS[status] || "";
  return `<span class="badge ${status}" title="${hint}">${labels[status] || status}</span>`;
}

function renderGames() {
  const q = $("#search").value.trim().toLowerCase();
  const ft = $("#filter-type").value;
  const fs = $("#filter-status").value;
  const tbody = $("#games tbody");
  tbody.innerHTML = "";
  let total = 0, native = 0, proton = 0, unlocked = 0;

  games.forEach((g) => {
    total++;
    if (g.game_type === "native") native++;
    if (g.game_type === "proton") proton++;
    if (g.installed) unlocked++;
    if (q && !g.name.toLowerCase().includes(q)) return;
    if (ft && g.game_type !== ft) return;
    if (fs === "none" && g.installed) return;
    if (fs === "installed" && !g.installed) return;

    const tr = document.createElement("tr");
    const actions = document.createElement("td");
    actions.className = "actions-cell";
    actions.innerHTML = `
      <button class="small primary" data-act="install">Установить</button>
      <button class="small" data-act="config">Настроить</button>
      <button class="small" data-act="dlc">DLC</button>
      <button class="small" data-act="folder">Папка</button>
      <button class="small danger" data-act="uninstall">Удалить</button>`;
    actions.querySelectorAll("button").forEach((btn) => {
      btn.addEventListener("click", () => onAction(btn.dataset.act, g));
    });
    tr.innerHTML = `
      <td><input type="checkbox" class="sel-game" data-appid="${g.appid}"></td>
      <td>${g.name}</td>
      <td class="muted">${g.appid}</td>
      <td>${badge(g.game_type)}</td>
      <td>${badge(g.installed)}</td>
      <td>${dlcBadge(g.dlc_status)}</td>`;
    tr.appendChild(actions);
    tbody.appendChild(tr);
  });
  const selAll = $("#sel-all");
  if (selAll) selAll.checked = false;
  updateSelection();

  $("#stat-total").textContent = total;
  $("#stat-native").textContent = native;
  $("#stat-proton").textContent = proton;
  $("#stat-unlocked").textContent = unlocked;
}

function refreshBusy(busy) {
  document.body.classList.toggle("busy", busy);
}

/* ------------------------------------------------------------------ log - */

function openLog() {
  $("#log-panel").classList.remove("hidden");
  if (logTimer) clearInterval(logTimer);
  logTimer = setInterval(pollLog, 500);
  pollLog();
}

function closeLog() {
  $("#log-panel").classList.add("hidden");
  if (logTimer) clearInterval(logTimer);
  logTimer = null;
}

async function pollLog() {
  try {
    const data = await api("/api/log");
    const el = $("#log-content");
    el.textContent = data.lines.join("\n");
    el.scrollTop = el.scrollHeight;
    refreshBusy(data.busy);
  } catch (e) { /* server restarting */ }
}

/* -------------------------------------------------------------- modals --- */

function openModal(title, bodyHtml, footHtml) {
  const root = $("#modal-root");
  root.innerHTML = `
    <div class="modal">
      <h2>${title}</h2>
      <div class="body">${bodyHtml}</div>
      <div class="foot">${footHtml}</div>
    </div>`;
  root.classList.add("open");
  return root;
}

function closeModal() {
  $("#modal-root").classList.remove("open");
  $("#modal-root").innerHTML = "";
}

/* ------------------------------------------------------------ selection -- */

function selectedGames() {
  const ids = [...document.querySelectorAll(".sel-game:checked")]
    .map((cb) => cb.dataset.appid);
  return games.filter((g) => ids.includes(g.appid));
}

function updateSelection() {
  const sel = selectedGames();
  const count = sel.length;
  $("#sel-count").textContent = count ? `Выбрано: ${count}` : "";
  $("#btn-install-sel").disabled = count === 0;
  $("#btn-uninstall-sel").disabled = count === 0;
}

/* -------------------------------------------------------------- actions -- */

function onAction(act, game) {
  if (act === "install") modalInstall([game]);
  else if (act === "uninstall") modalUninstall([game]);
  else if (act === "config") modalConfig(game);
  else if (act === "dlc") modalDlc(game);
  else if (act === "folder") api("/api/open-folder", {method: "POST",
    body: {appid: game.appid}}).catch((e) => alert(e.message));
}

function modalInstall(gamesSel) {
  const game = gamesSel[0];
  const isNative = game.game_type === "native";
  const title = gamesSel.length > 1
    ? `Установка — ${gamesSel.length} игр`
    : `Установка — ${game.name}`;
  const root = openModal(
    title,
    `<label class="row">Тип: ${badge(game.game_type)}</label>
     ${isNative ? `<p class="hint">creamlinux (LD_PRELOAD) будет скопирован в папку игры.
       Потребуется указать launch options: <b>sh ./cream.sh %command%</b></p>` : `
     <label class="row">Режим SmokeAPI
       <select id="mi-mode">
         <option value="hook">hook (по умолчанию)</option>
         <option value="koaloader">koaloader (переживает обновления)</option>
         <option value="proxy">proxy (максимально надёжно)</option>
       </select></label>
     <p class="hint">hook — DLL сама является прокси; koaloader — инжектор,
       работает после обновлений игры; proxy — заменяет steam_api, требует
       переустановки после обновлений.</p>`}
     <label class="row">unlockall (все DLC)
       <input type="checkbox" id="mi-unlockall" checked></label>
     <p class="hint">unlockall=true — игра считает установленными все свои DLC,
       список в cream_api.ini не нужен.</p>`,
    `<button id="mi-dry">Предпросмотр</button>
     <button id="mi-go" class="primary">Установить</button>
     <button id="mi-close">Отмена</button>`);

  const go = (dry) => {
    const body = {appids: gamesSel.map((g) => g.appid),
                  unlockall: $("#mi-unlockall").checked};
    if (!isNative) body.smokeapi_mode = $("#mi-mode").value;
    if (dry) body.dry_run = true;
    api("/api/install", {method: "POST", body})
      .then(() => { closeModal(); openLog(); })
      .catch((e) => alert(e.message));
  };
  $("#mi-dry").addEventListener("click", () => go(true));
  $("#mi-go").addEventListener("click", () => go(false));
  $("#mi-close").addEventListener("click", closeModal);
}

function modalUninstall(gamesSel) {
  const game = gamesSel[0];
  const title = gamesSel.length > 1
    ? `Удаление — ${gamesSel.length} игр`
    : `Удаление — ${game.name}`;
  const root = openModal(
    title,
    `<label class="row">Удалить также cream_api.ini
       <input type="checkbox" id="ui-ini" checked></label>
     <p class="hint">Для Proton будет восстановлен оригинальный steam_api(64).dll.</p>`,
    `<button id="ui-go" class="danger">Удалить</button>
     <button id="ui-close">Отмена</button>`);
  $("#ui-go").addEventListener("click", () => {
    api("/api/uninstall", {method: "POST",
      body: {appids: gamesSel.map((g) => g.appid),
             remove_ini: $("#ui-ini").checked}})
      .then(() => { closeModal(); openLog(); })
      .catch((e) => alert(e.message));
  });
  $("#ui-close").addEventListener("click", closeModal);
}

function modalConfig(game) {
  const root = openModal(
    `Настройки — ${game.name}`,
    `<div class="muted">Загрузка…</div>`,
    `<button id="cf-save" class="primary">Сохранить</button>
     <button id="cf-close">Закрыть</button>`);

  api("/api/game-config", {method: "POST", body: {appid: game.appid, read: true}})
    .then((ini) => {
      if (!ini || ini.error) {
        root.querySelector(".body").innerHTML =
          `<div class="muted">Нет cream_api.ini в этой игре — будет создан при
           сохранении.</div>`;
        return;
      }
      const cfg = ini.config || {};
      const dlc = ini.dlc || [];
      root.querySelector(".body").innerHTML = `
        <label class="row">unlockall
          <input type="checkbox" id="cf-unlockall" ${cfg.unlockall === "true" ? "checked" : ""}></label>
        <label class="row">logging
          <input type="checkbox" id="cf-logging" ${cfg.logging !== "false" ? "checked" : ""}></label>
        <div style="margin-top:10px"><b>DLC (${dlc.length})</b></div>
        <div class="dlc-list" id="cf-dlcs">${dlc.map(([id, name], i) => `
          <div><span>${id} — ${name}</span>
          <button class="small danger" data-i="${i}">✕</button></div>`).join("")}
        </div>
        <div style="margin-top:8px; display:flex; gap:6px">
          <input type="text" id="cf-new-id" placeholder="AppID">
          <input type="text" id="cf-new-name" placeholder="Название" style="flex:1">
          <button id="cf-add">+</button>
        </div>`;
      root.querySelectorAll("#cf-dlcs button").forEach((b) =>
        b.addEventListener("click", () => b.closest("div").remove()));
      $("#cf-add").addEventListener("click", () => {
        const id = $("#cf-new-id").value.trim();
        const name = $("#cf-new-name").value.trim();
        if (!id) return;
        const div = document.createElement("div");
        div.innerHTML = `<span>${id} — ${name || "?"}</span>
          <button class="small danger">✕</button>`;
        div.querySelector("button").addEventListener("click",
          () => div.remove());
        $("#cf-dlcs").appendChild(div);
        $("#cf-new-id").value = "";
        $("#cf-new-name").value = "";
      });
    })
    .catch(() => {
      root.querySelector(".body").innerHTML =
        `<div class="muted">Нет cream_api.ini в этой игре — будет создан при
         сохранении.</div>`;
    });

  $("#cf-save").addEventListener("click", () => {
    const dlcEl = $("#cf-dlcs");
    const dlc = dlcEl ? [...dlcEl.children].map((div) => {
      const text = div.querySelector("span").textContent;
      const [id, ...rest] = text.split(" — ");
      return [id.trim(), rest.join(" — ").trim()];
    }) : [];
    const body = {appid: game.appid, config: {
      unlockall: ($("#cf-unlockall") && $("#cf-unlockall").checked) ? "true" : "false",
      logging: ($("#cf-logging") && $("#cf-logging").checked) ? "true" : "false",
    }, dlc};
    api("/api/game-config", {method: "POST", body})
      .then(() => { closeModal(); refresh(); })
      .catch((e) => alert(e.message));
  });
  $("#cf-close").addEventListener("click", closeModal);
}

function modalDlc(game) {
  const root = openModal(
    `Обновление DLC — ${game.name}`,
    `<p class="hint">Загрузит актуальный список DLC игры из Steam Store API
       (по одному запросу на DLC, может занять время) и обновит cream_api.ini.</p>`,
    `<button id="dl-go" class="primary">Обновить</button>
     <button id="dl-close">Закрыть</button>`);
  $("#dl-go").addEventListener("click", () => {
    api("/api/dlc-update", {method: "POST", body: {appid: game.appid}})
      .then(() => { closeModal(); openLog(); })
      .catch((e) => alert(e.message));
  });
  $("#dl-close").addEventListener("click", closeModal);
}

/* ---------------------------------------------------------------- init --- */

async function refresh() {
  try {
    const data = await api("/api/games");
    games = data.games;
    refreshBusy(data.busy);
    renderGames();
  } catch (e) {
    $("#games tbody").innerHTML =
      `<tr><td colspan="5" class="muted">Ошибка: ${e.message}</td></tr>`;
  }
}

$("#btn-rescan").addEventListener("click", refresh);
$("#btn-logs").addEventListener("click", openLog);
$("#btn-log-close").addEventListener("click", closeLog);
$("#search").addEventListener("input", renderGames);
$("#filter-type").addEventListener("change", renderGames);
$("#filter-status").addEventListener("change", renderGames);
$("#sel-all").addEventListener("change", (e) => {
  document.querySelectorAll(".sel-game").forEach((cb) => {
    cb.checked = e.target.checked;
  });
  updateSelection();
});
$("#games tbody").addEventListener("change", (e) => {
  if (e.target.classList.contains("sel-game")) updateSelection();
});
$("#btn-install-sel").addEventListener("click", () => {
  const sel = selectedGames();
  if (sel.length) modalInstall(sel);
});
$("#btn-uninstall-sel").addEventListener("click", () => {
  const sel = selectedGames();
  if (sel.length) modalUninstall(sel);
});

refresh();
setInterval(() => {
  if (!logTimer) api("/api/log").then((d) => refreshBusy(d.busy)).catch(() => {});
}, 3000);
