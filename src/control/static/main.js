function $(id) { return document.getElementById(id); }

function setStatus(text) {
  $("status").textContent = text || "";
}

function setLog(text) {
  $("logBox").textContent = text || "";
}

function fmt01(x) {
  if (x === null || x === undefined) return "-";
  const v = Number(x);
  if (Number.isNaN(v)) return String(x);
  return v.toFixed(3);
}

function badge(text, cls) {
  return `<span class="badge ${cls || ""}">${text}</span>`;
}

async function jget(url) {
  const r = await fetch(url);
  return await r.json();
}

async function jpost(url, body) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {})
  });
  return await r.json();
}

let ALL_BACKENDS = [];

function renderTable(list) {
  const tbody = $("backendTable").querySelector("tbody");
  tbody.innerHTML = "";

  for (const b of list) {
    const tr = document.createElement("tr");

    const enabled = b.enabled ? badge("YES", "ok") : badge("NO", "danger");
    const warming = b.is_warming_up ? badge("YES", "warn") : badge("NO", "");
    const source = badge(b.source || "-", "");

    tr.innerHTML = `
      <td>${b.ip}</td>
      <td>${b.port}</td>
      <td>
        <input class="input input-small" data-ip="${b.ip}" data-port="${b.port}" value="${b.weight}" />
      </td>
      <td>${fmt01(b.gpu_usage)}</td>
      <td>${fmt01(b.vram_usage)}</td>
      <td>${enabled}</td>
      <td>${warming}</td>
      <td>${source}</td>
      <td>
        <button class="btn btn-secondary btnSave" data-ip="${b.ip}" data-port="${b.port}">保存</button>
      </td>
    `;
    tbody.appendChild(tr);
  }

  // 保存按钮绑定
  for (const btn of document.querySelectorAll(".btnSave")) {
    btn.onclick = async () => {
      const ip = btn.dataset.ip;
      const port = Number(btn.dataset.port);
      const inp = document.querySelector(`input[data-ip="${ip}"][data-port="${port}"]`);
      const weight = Number(inp.value || 0);

      setStatus("保存中…");
      const res = await jpost(`/api/backends/${ip}/${port}/weight`, { weight });
      setStatus(JSON.stringify(res.sync || res, null, 2));

      // 保存后顺便刷新一遍
      await refreshBackends();
    };
  }
}

function applyFilter() {
  const q = $("filterInput").value.trim();
  if (!q) return ALL_BACKENDS;

  return ALL_BACKENDS.filter(b => {
    const s = `${b.ip}:${b.port}`.toLowerCase();
    return s.includes(q.toLowerCase());
  });
}

async function refreshBackends() {
  setStatus("刷新中…");
  const data = await jget("/api/backends");
  if (!data.ok) {
    setStatus("刷新失败：" + JSON.stringify(data));
    return;
  }

  ALL_BACKENDS = data.backends || [];
  renderTable(applyFilter());

  setStatus(`后端数=${ALL_BACKENDS.length}  k8s_updated_at=${data.k8s_updated_at}  runtime_updated_at=${data.runtime_updated_at}`);
}

function openModal() {
  $("modal").classList.remove("hidden");
  $("pingOut").textContent = "";
  $("pingIp").focus();
}

function closeModal() {
  $("modal").classList.add("hidden");
}

async function doPing() {
  const ip = $("pingIp").value.trim();
  if (!ip) {
    $("pingOut").textContent = "请输入 IP";
    return;
  }
  $("pingOut").textContent = "ping 中…";
  setStatus("执行 ping…");

  const res = await jpost("/api/ping", { ip });
  const out = res.output || JSON.stringify(res, null, 2);

  $("pingOut").textContent = out;
  setLog(out);
  setStatus(res.ok ? "Ping 完成" : "Ping 失败");
}

async function loadAuditLog() {
  const tail = Number($("tailLines").value || 100);
  setStatus("加载审计日志…");
  const res = await jget(`/api/logs/audit?tail=${tail}`);

  if (!res.ok) {
    setLog("加载失败：" + JSON.stringify(res, null, 2));
    setStatus("加载失败");
    return;
  }
  setLog(res.data || "");
  setStatus(`audit.log 已加载（tail=${tail}）`);
}

async function doSync() {
  setStatus("手动同步中…");
  const res = await jpost("/api/sync", {});
  setStatus(JSON.stringify(res, null, 2));

  // 同步后刷新后端列表
  await refreshBackends();
}

window.onload = async () => {
  $("btnRefresh").onclick = refreshBackends;
  $("btnPing").onclick = openModal;

  $("btnClose").onclick = closeModal;
  $("btnDoPing").onclick = doPing;

  // 右上角“查看审计日志” => 直接加载到 logBox
  $("btnTailAudit").onclick = loadAuditLog;

  // 日志卡片里的“加载 audit.log”
  $("btnLoadAudit").onclick = loadAuditLog;

  // 手动同步按钮
  $("btnSync").onclick = doSync;

  $("filterInput").addEventListener("input", () => {
    renderTable(applyFilter());
  });

  await refreshBackends();
};
