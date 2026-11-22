const $ = (selector) => document.querySelector(selector);

const form = $("#searchForm");
const searchInput = $("#searchInput");
const searchButton = $(".search-button");
const resultsList = $("#resultsList");
const pipeline = $("#pipeline");

let mode = "keyword";
let results = [];
let page = 1;
let busy = false;
const pageSize = 100;

const pagination = document.createElement("div");
pagination.className = "pagination";
pagination.hidden = true;
resultsList.after(pagination);

async function api(path, options = {}) {
  const response = await fetch(path, options);
  const data = await response.json();
  if (!response.ok) throw new Error(data.message || "Server error");
  return data;
}

function escapeHtml(value) {
  const element = document.createElement("span");
  element.textContent = String(value);
  return element.innerHTML;
}

function renderResults(query, time) {
  const start = (page - 1) * pageSize;
  const visible = results.slice(start, start + pageSize);

  $("#resultsTitle").textContent = results.length
    ? `Results for “${query}”` : "No matching documents";
  $("#resultMeta").textContent =
    `${results.length} results · ${time} ms`;

  resultsList.innerHTML = visible.map((r, i) => `
    <article class="result-card">
      <span class="doc-number">${start + i + 1}</span>
      <div>
        <h3>Document ${escapeHtml(r.doc_id)}</h3>
        <p>${escapeHtml(r.snippet || "No preview available")}</p>
      </div>
      <span class="score">Score ${Number(r.score).toFixed(2)}</span>
    </article>
  `).join("") || '<p>No documents found. Try another query.</p>';

  const totalPages = Math.ceil(results.length / pageSize);
  pagination.hidden = totalPages <= 1;
  pagination.innerHTML = totalPages > 1 ? `
    <button id="prevPage" ${page === 1 ? "disabled" : ""}>Previous</button>
    <span>Page ${page} of ${totalPages}</span>
    <button id="nextPage" ${page === totalPages ? "disabled" : ""}>Next</button>
  ` : "";

  if (totalPages > 1) {
    $("#prevPage").onclick = () => { page--; renderResults(query, time); };
    $("#nextPage").onclick = () => { page++; renderResults(query, time); };
  }
}

function addStage(label) {
  const step = document.createElement("span");
  step.className = "pipeline-step";
  step.textContent = label;
  pipeline.append(step);
}

function search(query) {
  if (!query || busy) return;
  busy = true;
  results = [];
  page = 1;
  pagination.hidden = true;
  resultsList.innerHTML = "";
  pipeline.innerHTML = "";
  pipeline.hidden = false;
  $("#resultsTitle").textContent = "Searching...";
  $("#resultMeta").textContent = "";
  searchButton.disabled = true;

  const scheme = location.protocol === "https:" ? "wss:" : "ws:";
  const socket = new WebSocket(`${scheme}//${location.host}/ws/search`);
  let finished = false;

  function finish(error = "") {
    if (finished) return;
    finished = true;
    busy = false;
    searchButton.disabled = false;
    if (error) {
      $("#resultsTitle").textContent = "Search failed";
      resultsList.textContent = error;
    }
    socket.close();
  }

  socket.onopen = () => {
    socket.send(JSON.stringify({ query, mode }));
    addStage("Query sent");
  };

  socket.onmessage = (event) => {
    let message;
    try {
      message = JSON.parse(event.data);
    } catch {
      finish("Invalid server response");
      return;
    }

    if (message.stage === "query_expansion") {
      if (mode === "expanded") addStage("Query expansion complete");
      for (const term of message.data?.expanded_terms || []) {
        const tag = document.createElement("span");
        tag.className = "expanded-term";
        tag.textContent = `+ ${term}`;
        pipeline.append(tag);
      }
    } else if (message.stage === "keyword_search") {
      results = message.data?.results || [];
      addStage("Index searched");
      renderResults(query, message.data?.time_ms || 0);
    } else if (message.stage === "done") {
      addStage("Done");
      finish();
    } else if (message.stage === "error") {
      finish(message.data?.message || "Search failed");
    }
  };

  socket.onerror = () => finish("Cannot connect to WebSocket server");
  socket.onclose = () => {
    if (!finished) finish("WebSocket connection closed");
  };
}

async function loadStats() {
  try {
    const stats = await api("/api/stats");
    $("#documentsStat").textContent = stats.documents;
    $("#buildTimeStat").textContent = `${stats.index_build_time_ms} ms`;
    $("#threadsStat").textContent = stats.threads_used;
  } catch {
    $("#documentsStat").textContent = "—";
    $("#buildTimeStat").textContent = "—";
    $("#threadsStat").textContent = "—";
  }
}

form.onsubmit = (event) => {
  event.preventDefault();
  search(searchInput.value.trim());
};

document.querySelectorAll(".mode-button").forEach((button) => {
  button.onclick = () => {
    mode = button.dataset.mode;
    document.querySelectorAll(".mode-button").forEach((b) =>
      b.classList.toggle("is-active", b === button));
  };
});

$("#openIndexButton").onclick = () => $("#indexDialog").showModal();
$("#threadRange").oninput = (event) => {
  $("#threadOutput").value = event.target.value;
};

$("#buildIndexButton").onclick = async () => {
  const button = $("#buildIndexButton");
  button.disabled = true;
  $("#indexMessage").textContent = "Building index...";
  try {
    const data = await api("/api/index", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ threads: Number($("#threadRange").value) })
    });
    $("#indexMessage").textContent =
      `Indexed ${data.documents_indexed} documents in ${data.time_ms} ms`;
    await loadStats();
  } catch (error) {
    $("#indexMessage").textContent = error.message;
  } finally {
    button.disabled = false;
  }
};

loadStats();