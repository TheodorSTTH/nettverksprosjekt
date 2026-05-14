export interface GCounter {
  increment(nodeId: string, amount: number): void
  merge(other: GCounter): void
  value(): number
  delete(): void
}

export interface CrdtModule {
  GCounter: new () => GCounter
}

type CrdtModuleFactory = (options?: {
  locateFile?: (path: string, prefix: string) => string
}) => Promise<CrdtModule>

const crdtModulePath = '/wasm/crdt.mjs'

let crdtModulePromise: Promise<CrdtModule> | null = null

export function loadCrdtModule() {
  if (!crdtModulePromise) {
    crdtModulePromise = import(/* @vite-ignore */ crdtModulePath).then(
      ({ default: createCrdtModule }: { default: CrdtModuleFactory }) =>
        createCrdtModule({
          locateFile: (path) => `/wasm/${path}`,
        }),
    )
  }

  return crdtModulePromise
}

export async function createGCounter(): Promise<GCounter> {
  const module = await loadCrdtModule()
  return new module.GCounter()
}
