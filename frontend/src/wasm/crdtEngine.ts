export type RgaId = {
  peer_id: number
  sequence: number
}

export type RgaNode = {
  value: string
  id: RgaId
  parent_id: RgaId
  tombstoned: boolean
}

export type InsertMessage = {
  type: 'insert' | 'remote_insert'
  value: string
  char: string
  id: RgaId
  parent_id: RgaId
}

export type DeleteMessage = {
  type: 'delete' | 'remote_delete'
  target_id: RgaId
}

export type ErrorMessage = {
  type: 'error'
  message: string
}

export type CrdtOperation = InsertMessage | DeleteMessage

type WasmFactoryModule = {
  default?: (options?: Record<string, unknown>) => Promise<WasmModule>
}

type WasmModule = {
  CrdtDocument: new (peerId: number) => WasmDocument
}

type WasmDocument = {
  setPeerId(peerId: number): void
  loadSnapshot(nodes: RgaNode[]): void
  localInsert(value: string, parentId: RgaId): InsertMessage | ErrorMessage
  localDelete(targetId: RgaId): DeleteMessage | ErrorMessage
  applyRemoteInsert(operation: InsertMessage): boolean
  applyRemoteDelete(operation: DeleteMessage): boolean
  toString(): string
  visibleIds(): RgaId[]
  nodesJson(): RgaNode[]
  delete?: () => void
}

export type CrdtEngine = {
  setPeerId(peerId: number): void
  loadSnapshot(nodes: RgaNode[]): void
  localInsert(value: string, parentId: RgaId): InsertMessage | ErrorMessage
  localDelete(targetId: RgaId): DeleteMessage | ErrorMessage
  applyRemoteInsert(operation: InsertMessage): boolean
  applyRemoteDelete(operation: DeleteMessage): boolean
  toString(): string
  visibleIds(): RgaId[]
  nodesJson(): RgaNode[]
  destroy(): void
}

export function rootId(): RgaId {
  return { peer_id: 0, sequence: 0 }
}

export function isErrorMessage(value: unknown): value is ErrorMessage {
  return (
    typeof value === 'object' &&
    value !== null &&
    'type' in value &&
    (value as { type?: unknown }).type === 'error'
  )
}

export async function createCrdtEngine(): Promise<CrdtEngine> {
  const wasmModulePath = new URL('/wasm/crdt_wasm.js', window.location.origin).href
  const nativeImport = new Function('path', 'return import(path)') as (
    path: string,
  ) => Promise<WasmFactoryModule>
  const imported = await nativeImport(wasmModulePath)
  const createModule = imported.default

  if (!createModule) {
    throw new Error('CRDT WebAssembly module did not export a default factory')
  }

  const module = await createModule({
    locateFile: (path: string) => `/wasm/${path}`,
  })
  const document = new module.CrdtDocument(0)

  return {
    setPeerId(peerId) {
      document.setPeerId(peerId)
    },
    loadSnapshot(nodes) {
      document.loadSnapshot(nodes)
    },
    localInsert(value, parentId) {
      return document.localInsert(value, parentId)
    },
    localDelete(targetId) {
      return document.localDelete(targetId)
    },
    applyRemoteInsert(operation) {
      return document.applyRemoteInsert(operation)
    },
    applyRemoteDelete(operation) {
      return document.applyRemoteDelete(operation)
    },
    toString() {
      return document.toString()
    },
    visibleIds() {
      return document.visibleIds()
    },
    nodesJson() {
      return document.nodesJson()
    },
    destroy() {
      document.delete?.()
    },
  }
}
