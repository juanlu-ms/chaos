const statusProject = document.getElementById("status-project");
const statusMessage = document.getElementById("status-message");
const statusChip = document.getElementById("status-chip");
const lastUpdated = document.getElementById("last-updated");
const containerList = document.getElementById("container-list");
const containerCount = document.getElementById("container-count");
const containerEmpty = document.getElementById("container-empty");
const containerError = document.getElementById("container-error");
const refreshBtn = document.getElementById("refresh-btn");
const toggleAutoBtn = document.getElementById("toggle-auto");

let autoRefresh = true;
let timerId = null;

const formatNow = () => new Date().toLocaleTimeString("es-ES", {
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
});

const setLoading = () => {
  statusChip.textContent = "Cargando";
  statusChip.style.color = "#f5a623";
  statusChip.style.background = "rgba(245, 166, 35, 0.2)";
};

const setOnline = () => {
  statusChip.textContent = "Online";
  statusChip.style.color = "#36f9c4";
  statusChip.style.background = "rgba(54, 249, 196, 0.2)";
};

const setOffline = () => {
  statusChip.textContent = "Offline";
  statusChip.style.color = "#f87171";
  statusChip.style.background = "rgba(248, 113, 113, 0.2)";
};

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const invokeAction = async (containerId, action) => {
  const url = `/containers/${encodeURIComponent(containerId)}/${action}`;
  let lastError = null;

  for (let attempt = 0; attempt < 2; attempt += 1) {
    try {
      const response = await fetch(url, { method: "POST" });
      const text = await response.text();
      let payload = {};
      if (text) {
        try {
          payload = JSON.parse(text);
        } catch (error) {
          payload = {};
        }
      }

      if (!response.ok) {
        const reason = payload.error || text || `HTTP ${response.status}`;
        const err = new Error(reason);
        err.status = response.status;
        throw err;
      }

      return payload;
    } catch (error) {
      lastError = error;
      const status = typeof error.status === "number" ? error.status : null;
      const shouldRetry = status === null || status >= 500;
      if (attempt === 0 && shouldRetry) {
        await delay(700);
        continue;
      }
      break;
    }
  }

  throw lastError || new Error("Action failed");
};

const renderContainers = (containers) => {
  containerList.innerHTML = "";
  containerError.style.display = "none";

  if (!containers.length) {
    containerEmpty.style.display = "block";
    containerCount.textContent = "0";
    return;
  }

  containerEmpty.style.display = "none";
  containerCount.textContent = String(containers.length);

  containers.forEach((container) => {
    const item = document.createElement("li");
    item.className = "container-item";

    const name = document.createElement("div");
    name.className = "name";
    name.textContent = container.name || "(sin nombre)";

    const meta = document.createElement("div");
    meta.className = "meta";
    meta.textContent = container.id || "unknown";

    const state = document.createElement("div");
    const normalized = (container.state || "unknown").toLowerCase();
    state.className = `state ${normalized}`;
    state.textContent = container.state || "unknown";

    const actions = document.createElement("div");
    actions.className = "container-actions";

    const stopBtn = document.createElement("button");
    stopBtn.className = "action-btn";
    stopBtn.textContent = "Stop";

    const killBtn = document.createElement("button");
    killBtn.className = "action-btn danger";
    killBtn.textContent = "Kill";

    stopBtn.addEventListener("click", async () => {
      stopBtn.disabled = true;
      killBtn.disabled = true;
      statusMessage.textContent = `Stop requested: ${container.name || container.id}`;
      try {
        await invokeAction(container.id, "stop");
        await refresh();
      } catch (error) {
        statusMessage.textContent = `Stop failed: ${error.message}`;
      } finally {
        stopBtn.disabled = false;
        killBtn.disabled = false;
      }
    });

    killBtn.addEventListener("click", async () => {
      if (!window.confirm(`Kill container ${container.name || container.id}?`)) {
        return;
      }
      stopBtn.disabled = true;
      killBtn.disabled = true;
      statusMessage.textContent = `Kill requested: ${container.name || container.id}`;
      try {
        await invokeAction(container.id, "kill");
        await refresh();
      } catch (error) {
        statusMessage.textContent = `Kill failed: ${error.message}`;
      } finally {
        stopBtn.disabled = false;
        killBtn.disabled = false;
      }
    });

    actions.appendChild(stopBtn);
    actions.appendChild(killBtn);

    item.appendChild(name);
    item.appendChild(meta);
    item.appendChild(state);
    item.appendChild(actions);
    containerList.appendChild(item);
  });
};

const loadStatus = async () => {
  const response = await fetch("/status");
  if (!response.ok) {
    throw new Error("status not ok");
  }
  return response.json();
};

const loadContainers = async () => {
  const response = await fetch("/containers");
  if (!response.ok) {
    throw new Error("containers not ok");
  }
  return response.json();
};

const refresh = async () => {
  setLoading();
  try {
    const [status, containers] = await Promise.all([loadStatus(), loadContainers()]);
    statusProject.textContent = status.project || "CHAOS";
    statusMessage.textContent = status.status || "Online";
    lastUpdated.textContent = formatNow();
    setOnline();
    renderContainers(Array.isArray(containers) ? containers : []);
  } catch (error) {
    setOffline();
    statusProject.textContent = "CHAOS";
    statusMessage.textContent = "Sin respuesta del adaptador web";
    lastUpdated.textContent = formatNow();
    containerList.innerHTML = "";
    containerEmpty.style.display = "none";
    containerError.style.display = "block";
    containerCount.textContent = "0";
  }
};

const startAutoRefresh = () => {
  if (timerId) {
    clearInterval(timerId);
  }
  timerId = setInterval(refresh, 5000);
};

refreshBtn.addEventListener("click", refresh);

toggleAutoBtn.addEventListener("click", () => {
  autoRefresh = !autoRefresh;
  toggleAutoBtn.textContent = `Auto: ${autoRefresh ? "ON" : "OFF"}`;
  if (autoRefresh) {
    startAutoRefresh();
  } else if (timerId) {
    clearInterval(timerId);
    timerId = null;
  }
});

refresh();
startAutoRefresh();
