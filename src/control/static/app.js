async function jget(url) {
  const r = await fetch(url);
  return await r.json();
}

async function jpost(url, body) {
  const r = await fetch(url, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(body || {})
  });
  return await r.json();
}

function $(id){ return document.getElementById(id); }

function setStatus(obj){
  $("status").textContent = JSON.stringify(obj, null, 2);
}

function fmt01(x){
  if (x === null || x === undefined) return "-";
  const v = Number(x);
  if (Number.isNaN(v)) return String(x);
  return v.toFixed(3);
}

function renderBackends(data){
  const tbody = $("backendTbody");
  tbody.innerHTML = "";

  const meta = `k8s_updated_at=${data.k8s_updated_at}  runtime_updated_at=${data.runtime_updated_at}  now=${data.updated_at}`;
  $("backendMeta").textContent = meta;

  for (const b of data.backends || []) {
    const tr = document.createElement("tr");

    tr.innerHTML = `
      <td>${b.ip}</td>
      <td>${b.port}</td>
      <td>${b.source}</td>
      <td>${b.enabled ? "YES" : "NO"}</td>
      <td>${b.is_warming_up ? "YES" : "NO"}</td>
      <td>${fmt01(b.gpu_usage)}</td>
      <td>${fmt01(b.vram_usage)}</td>
      <td><input data-ip="${b.ip}" data-port="${b.port}" class="winput" value="${b.weight}"></td>
      <td><button class="btnSave" data-ip="${b.ip}" data-port="${b.port}">保存</button></td>
    `;
    tbody.appendChild(tr);
  }

  // 绑定保存按钮
  for (const btn of document.querySelectorAll(".btnSave")) {
    btn.onclick = async () => {
      const ip = btn.dataset.ip;
      const port = Number(btn.dataset.port);
      const inp = document.querySelector(`input.winput[data-ip="${ip}"][data-port="${port}"]`);
      const weight = Number(inp.value || 0);

      const res = await jpost(`/api/backends/${ip}/${port}/weight`, {weight});
      setStatus(res);
    };
  }
}

async function refreshBackends(){
  const data = await jget("/api/backends");
  renderBackends(data);
}

async function doPing(){
  const ip = $("pingIp").value.trim();
  if (!ip) { $("pingOut").textContent = "请输入 IP"; return; }
  $("pingOut").textContent = "ping中...";
  const res = await jpost("/api/ping", {ip});
  $("pingOut").textContent = res.output || JSON.stringify(res, null, 2);
  setStatus(res);
}

async function loadLog(){
  const tail = Number($("logTail").value || 100);
  $("logOut").textContent = "加载中...";
  const res = await jget(`/api/logs/audit?tail=${tail}`);
  $("logOut").textContent = res.data || "";
  setStatus(res);
}

async function doSync(){
  const res = await jpost("/api/sync", {});
  setStatus(res);
  await refreshBackends();
}

let timer = null;

window.onload = async () => {
  $("btnRefresh").onclick = refreshBackends;
  $("btnPing").onclick = doPing;
  $("btnLoadLog").onclick = loadLog;
  $("btnSync").onclick = doSync;

  $("chkAutoRefresh").onchange = async (e) => {
    if (e.target.checked) {
      await refreshBackends();
      timer = setInterval(refreshBackends, 2000);
    } else {
      if (timer) clearInterval(timer);
      timer = null;
    }
  };

  await refreshBackends();
};
