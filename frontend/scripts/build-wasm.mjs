import { mkdirSync, readdirSync } from 'node:fs'
import { basename, dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'

const __dirname = dirname(fileURLToPath(import.meta.url))
const frontendRoot = resolve(__dirname, '..')
const repoRoot = resolve(frontendRoot, '..')
const outDir = resolve(frontendRoot, 'public', 'wasm')
const outFile = resolve(outDir, 'crdt_wasm.js')
const crdtSources = readdirSync(resolve(repoRoot, 'shared', 'crdt'))
  .filter((entry) => entry.endsWith('.cpp'))
  .sort()
  .map((entry) => resolve(repoRoot, 'shared', 'crdt', entry))

mkdirSync(outDir, { recursive: true })

const emccArgs = [
  resolve(repoRoot, 'shared', 'wasm', 'rga_wasm_bindings.cpp'),
  ...crdtSources,
  '-I',
  resolve(repoRoot, 'shared'),
  '--bind',
  '-std=c++17',
  '-O2',
  '-sMODULARIZE=1',
  '-sEXPORT_ES6=1',
  '-sEXPORT_NAME=createCrdtWasmModule',
  '-sENVIRONMENT=web',
  '-sALLOW_MEMORY_GROWTH=1',
  '-sFILESYSTEM=0',
  '-o',
  outFile,
]

function run(command, args, options = {}) {
  return spawnSync(command, args, {
    stdio: 'inherit',
    ...options,
  })
}

const local = run('emcc', emccArgs)
if (local.status === 0) {
  process.exit(0)
}

const localFailedBecauseMissing =
  local.error && ['ENOENT', 'EACCES'].includes(local.error.code ?? '')

if (!localFailedBecauseMissing) {
  process.exit(local.status ?? 1)
}

console.warn('emcc was not found locally. Trying Docker image emscripten/emsdk:3.1.74...')

const uid = typeof process.getuid === 'function' ? `${process.getuid()}:${process.getgid()}` : undefined
const dockerArgs = [
  'run',
  '--rm',
  '-v',
  `${repoRoot}:/src`,
  '-w',
  '/src',
]

if (uid) {
  dockerArgs.push('--user', uid)
}

dockerArgs.push(
  'emscripten/emsdk:3.1.74',
  'emcc',
  'shared/wasm/rga_wasm_bindings.cpp',
  ...crdtSources.map((source) => `shared/crdt/${basename(source)}`),
  '-I',
  'shared',
  '--bind',
  '-std=c++17',
  '-O2',
  '-sMODULARIZE=1',
  '-sEXPORT_ES6=1',
  '-sEXPORT_NAME=createCrdtWasmModule',
  '-sENVIRONMENT=web',
  '-sALLOW_MEMORY_GROWTH=1',
  '-sFILESYSTEM=0',
  '-o',
  'frontend/public/wasm/crdt_wasm.js',
)

const docker = run('docker', dockerArgs)
if (docker.status !== 0) {
  console.error(
    'Failed to build CRDT WebAssembly. Install Emscripten so `emcc` is on PATH, or make Docker available.',
  )
}

process.exit(docker.status ?? 1)
