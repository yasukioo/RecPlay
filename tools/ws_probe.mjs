// Quick WS probe — captures messages from /api/ws for N seconds.
import WebSocket from "ws";

const url = "ws://localhost:8080/api/ws";
const ms = Number(process.argv[2] ?? 6000);

const types = new Map();
const ws = new WebSocket(url);

ws.on("open", () => {
  console.log(`[open] ${url} — collecting for ${ms}ms`);
});

ws.on("message", (data) => {
  let parsed;
  try { parsed = JSON.parse(data.toString()); } catch { parsed = { type: "<unparsed>" }; }
  const t = parsed.type ?? "<no-type>";
  if (!types.has(t)) types.set(t, []);
  types.get(t).push(parsed);
});

ws.on("error", (err) => { console.error("[error]", err.message); });
ws.on("close", () => { console.log("[close]"); });

setTimeout(() => {
  for (const [t, arr] of types) {
    console.log(`\n== type=${t} count=${arr.length} ==`);
    // show first + last sample per type
    console.log("first:", JSON.stringify(arr[0]));
    if (arr.length > 1) console.log("last :", JSON.stringify(arr[arr.length - 1]));
  }
  ws.close();
  process.exit(0);
}, ms);
