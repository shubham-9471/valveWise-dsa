const http = require('node:http');
const fs = require('node:fs');
const fsp = fs.promises;
const path = require('node:path');
const os = require('node:os');
const { spawn } = require('node:child_process');

const ROOT = path.resolve(__dirname, '..');
const FRONTEND = path.join(ROOT, 'frontend');
const DATA = path.join(ROOT, 'data');
const PORT = Number.parseInt(process.env.PORT || '3000', 10) || 3000;
const ID = /^[A-Za-z0-9_.-]+$/;
const MAX_INP_BYTES = 2 * 1024 * 1024;
const MAX_BODY_BYTES = 3 * 1024 * 1024;
const RUN_TIMEOUT_MS = 30_000;

function send(res, status, payload, type = 'application/json; charset=utf-8') {
  const body = type.startsWith('application/json') ? JSON.stringify(payload) : payload;
  res.writeHead(status, { 'Content-Type': type, 'Content-Length': Buffer.byteLength(body), 'Cache-Control': 'no-store' });
  res.end(body);
}

function errorPayload(code, message, extra = {}) { return { error: { code, message, ...extra } }; }

async function readJson(req) {
  let size = 0; const chunks = [];
  for await (const chunk of req) { size += chunk.length; if (size > MAX_BODY_BYTES) { const error = new Error('Request body exceeds 3 MB.'); error.statusCode = 413; throw error; } chunks.push(chunk); }
  try { return JSON.parse(Buffer.concat(chunks).toString('utf8')); } catch { const error = new Error('Request body must be valid JSON.'); error.statusCode = 400; throw error; }
}

function validateIds(value, field) {
  if (value == null) return [];
  if (!Array.isArray(value) || value.some(id => typeof id !== 'string' || !ID.test(id))) throw Object.assign(new Error(`${field} must be an array of safe IDs.`), { statusCode: 400 });
  return value;
}

function validateRun(body, networkNames) {
  const fail = validateIds(body.fail, 'fail'); const stuck = validateIds(body.stuck, 'stuck'); const valves = validateIds(body.valves, 'valves');
  const budget = body.budget == null ? 3 : body.budget;
  if (!Number.isInteger(budget) || budget < 0 || budget > 50) throw Object.assign(new Error('budget must be an integer from 0 to 50.'), { statusCode: 400 });
  if (body.seed != null && (!Number.isInteger(body.seed) || body.seed < 0 || body.seed > 0xffffffff)) throw Object.assign(new Error('seed must be an unsigned integer.'), { statusCode: 400 });
  let networkName = null; let inputText = null;
  const supplied = body.network ?? body.networkText ?? body.inpText;
  if (typeof supplied !== 'string' || !supplied.trim()) throw Object.assign(new Error('Provide a network filename or uploaded .inp text.'), { statusCode: 400 });
  if (/\[\s*(TITLE|JUNCTIONS|PIPES|RESERVOIRS|TANKS)\s*\]/i.test(supplied) || supplied.includes('\n')) {
    inputText = supplied;
    if (Buffer.byteLength(inputText, 'utf8') > MAX_INP_BYTES) throw Object.assign(new Error('Uploaded .inp file exceeds the 2 MB limit.'), { statusCode: 413 });
  } else {
    networkName = path.basename(supplied);
    if (networkName !== supplied || !/^[A-Za-z0-9_.-]+\.inp$/i.test(networkName) || !networkNames.includes(networkName)) throw Object.assign(new Error('Unknown network. Choose a filename returned by GET /api/networks.'), { statusCode: 400 });
  }
  return { fail, stuck, valves, budget, seed: body.seed, networkName, inputText };
}

async function networkNames() { return (await fsp.readdir(DATA, { withFileTypes: true })).filter(entry => entry.isFile() && /\.inp$/i.test(entry.name)).map(entry => entry.name).sort(); }

async function runBackend(options, names) {
  const bin = process.env.VALVEWISE_BIN || path.join(ROOT, 'backend', process.platform === 'win32' ? 'valvewise.exe' : 'valvewise');
  try { await fsp.access(bin, fs.constants.X_OK); } catch { throw Object.assign(new Error(`ValveWise binary is missing: ${bin}`), { statusCode: 503, code: 'BINARY_MISSING', hint: 'Build it with make -C backend (or set VALVEWISE_BIN).' }); }
  const temp = await fsp.mkdtemp(path.join(os.tmpdir(), 'valvewise-run-')); const output = path.join(temp, 'result.json'); let input = options.networkName ? path.join(DATA, options.networkName) : path.join(temp, 'upload.inp');
  try {
    if (options.inputText != null) await fsp.writeFile(input, options.inputText, 'utf8');
    const args = ['--input', input, '--output', output, '--budget', String(options.budget), '--verbose'];
    if (options.fail.length) args.push('--fail', options.fail.join(','));
    if (options.stuck.length) args.push('--stuck', options.stuck.join(','));
    if (options.seed != null) args.push('--seed', String(options.seed));
    if (options.valves.length) args.push('--valves', options.valves.join(','));
    const result = await new Promise((resolve, reject) => {
      const child = spawn(bin, args, { cwd: ROOT, windowsHide: true }); let stderr = ''; let stdout = ''; let settled = false;
      const finish = (fn, value) => { if (settled) return; settled = true; clearTimeout(timer); fn(value); };
      child.stderr.on('data', chunk => { stderr += chunk.toString(); if (stderr.length > 64 * 1024) stderr = stderr.slice(-64 * 1024); });
      child.stdout.on('data', chunk => { stdout += chunk.toString(); });
      child.on('error', error => finish(reject, Object.assign(new Error(`Could not start ValveWise: ${error.message}`), { statusCode: 503, code: 'SPAWN_FAILED', hint: 'Build it with make -C backend (or set VALVEWISE_BIN).' })));
      child.on('close', code => finish(code === 0 ? resolve : reject, code === 0 ? { stderr, stdout } : Object.assign(new Error(`ValveWise exited with code ${code}.`), { statusCode: 502, code: 'BACKEND_FAILED', stderr, stdout })));
      const timer = setTimeout(() => { child.kill('SIGTERM'); finish(reject, Object.assign(new Error('ValveWise timed out after 30 seconds.'), { statusCode: 504, code: 'BACKEND_TIMEOUT', stderr })); }, RUN_TIMEOUT_MS);
    });
    let json; try { json = JSON.parse(await fsp.readFile(output, 'utf8')); } catch { throw Object.assign(new Error('ValveWise did not produce valid JSON.'), { statusCode: 502, code: 'INVALID_OUTPUT', stderr: result.stderr, stdout: result.stdout }); }
    return json;
  } finally { await fsp.rm(temp, { recursive: true, force: true }); }
}

function contentType(file) { return { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml' }[path.extname(file).toLowerCase()] || 'application/octet-stream'; }
async function serveStatic(url, res) {
  let relative = decodeURIComponent(url.pathname.slice(1)); if (!relative) relative = 'index.html'; const file = path.resolve(FRONTEND, relative);
  if (file !== FRONTEND && !file.startsWith(`${FRONTEND}${path.sep}`)) return send(res, 403, errorPayload('FORBIDDEN', 'Path is outside frontend.'));
  try { const stat = await fsp.stat(file); if (!stat.isFile()) throw new Error(); send(res, 200, await fsp.readFile(file), contentType(file)); } catch { send(res, 404, errorPayload('NOT_FOUND', 'Frontend file not found.')); }
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  try {
    if (req.method === 'GET' && url.pathname === '/api/networks') return send(res, 200, { networks: await networkNames() });
    if (req.method === 'POST' && url.pathname === '/api/run') { const body = await readJson(req); const names = await networkNames(); const options = validateRun(body, names); return send(res, 200, await runBackend(options, names)); }
    if (req.method === 'GET') return serveStatic(url, res);
    return send(res, 405, errorPayload('METHOD_NOT_ALLOWED', 'Method not allowed.'));
  } catch (error) { return send(res, error.statusCode || 500, errorPayload(error.code || 'REQUEST_FAILED', error.message, { ...(error.hint ? { hint: error.hint } : {}), ...(error.stderr ? { stderr: error.stderr } : {}) })); }
});

server.listen(PORT, () => console.log(`ValveWise server listening on http://localhost:${PORT}`));
