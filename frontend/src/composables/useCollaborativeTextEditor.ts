import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import {
  createCrdtEngine,
  isErrorMessage,
  rootId,
  type CrdtEngine,
  type DeleteMessage,
  type InsertMessage,
  type RgaId,
  type RgaNode,
} from '../wasm/crdtEngine'

export type SocketState = 'connecting' | 'connected' | 'disconnected'

type AssignPeerMessage = {
  type: 'assign_peer_id'
  peer_id: number
}

type SyncStateMessage = {
  type: 'sync_state'
  nodes: RgaNode[]
  visible_text?: string
}

type ServerErrorMessage = {
  type: 'error'
  message?: string
}

type ServerMessage = AssignPeerMessage | SyncStateMessage | InsertMessage | DeleteMessage | ServerErrorMessage

const defaultText = ''

function nowLabel() {
  return new Intl.DateTimeFormat(undefined, {
    hour: '2-digit',
    minute: '2-digit',
  }).format(new Date())
}

function commonPrefixLength(left: string, right: string) {
  const max = Math.min(left.length, right.length)
  let index = 0

  while (index < max && left[index] === right[index]) {
    index += 1
  }

  return index
}

function commonSuffixLength(left: string, right: string, prefixLength: number) {
  const max = Math.min(left.length - prefixLength, right.length - prefixLength)
  let count = 0

  while (
    count < max &&
    left[left.length - 1 - count] === right[right.length - 1 - count]
  ) {
    count += 1
  }

  return count
}

export function useCollaborativeTextEditor() {
  const editorText = ref(defaultText)
  const serverUrl = ref('ws://localhost:3030/ws')
  const socketState = ref<SocketState>('disconnected')
  const statusMessage = ref('Loading CRDT WebAssembly')
  const hasUnsavedChanges = ref(false)
  const lastSavedAt = ref<string | null>(null)
  const peerId = ref<number | null>(null)
  const wasmReady = ref(false)

  const history = ref<string[]>([defaultText])
  const historyIndex = ref(0)

  let socket: WebSocket | null = null
  let engine: CrdtEngine | null = null
  let isApplyingHistory = false
  let isDestroyed = false

  const canUndo = computed(() => historyIndex.value > 0)
  const canRedo = computed(() => historyIndex.value < history.value.length - 1)
  const isConnected = computed(() => socketState.value === 'connected')

  const saveLabel = computed(() => {
    if (!wasmReady.value) {
      return 'CRDT loading'
    }

    if (hasUnsavedChanges.value) {
      return 'Local change pending'
    }

    if (lastSavedAt.value) {
      return `Synced ${lastSavedAt.value}`
    }

    return 'Synced automatically'
  })

  const connectionLabel = computed(() => {
    if (socketState.value === 'connected') {
      return peerId.value === null ? 'Connected' : `Connected as ${peerId.value}`
    }

    if (socketState.value === 'connecting') {
      return 'Connecting'
    }

    return 'Disconnected'
  })

  function recordHistory(value: string) {
    if (isApplyingHistory) {
      return
    }

    const current = history.value[historyIndex.value]

    if (current === value) {
      return
    }

    history.value = [...history.value.slice(0, historyIndex.value + 1), value]
    historyIndex.value = history.value.length - 1
  }

  function sendToServer(payload: Record<string, unknown>) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      hasUnsavedChanges.value = true
      statusMessage.value = 'Editing locally; reconnect to sync'
      return false
    }

    socket.send(JSON.stringify(payload))
    lastSavedAt.value = nowLabel()
    hasUnsavedChanges.value = false
    return true
  }

  function requestFullSync() {
    sendToServer({ type: 'sync_request' })
  }

  function saveDocument() {
    requestFullSync()
    statusMessage.value = isConnected.value ? 'Requested latest server state' : 'Reconnect to sync'
  }

  async function initializeEngine() {
    if (engine) {
      return true
    }

    try {
      engine = await createCrdtEngine()
      wasmReady.value = true
      statusMessage.value = 'CRDT ready'
      return true
    } catch (error) {
      wasmReady.value = false
      statusMessage.value =
        error instanceof Error
          ? `CRDT WebAssembly unavailable: ${error.message}`
          : 'CRDT WebAssembly unavailable'
      return false
    }
  }

  function visibleIds(): RgaId[] {
    return engine?.visibleIds() ?? []
  }

  function applyLocalTextChange(nextText: string) {
    if (!engine) {
      statusMessage.value = 'CRDT WebAssembly is not ready'
      return false
    }

    const previousText = engine.toString()
    if (previousText === nextText) {
      return true
    }

    const idsBeforeChange = visibleIds()
    const prefixLength = commonPrefixLength(previousText, nextText)
    const suffixLength = commonSuffixLength(previousText, nextText, prefixLength)
    const deleteCount = previousText.length - prefixLength - suffixLength
    const insertText = nextText.slice(prefixLength, nextText.length - suffixLength)

    if (prefixLength + deleteCount > idsBeforeChange.length) {
      requestFullSync()
      statusMessage.value = 'Local edit could not be mapped; requested full sync'
      return false
    }

    const deletedIds = idsBeforeChange.slice(prefixLength, prefixLength + deleteCount).reverse()
    for (const id of deletedIds) {
      const operation = engine.localDelete(id)
      if (isErrorMessage(operation)) {
        requestFullSync()
        statusMessage.value = operation.message
        return false
      }

      sendToServer(operation)
    }

    let parentId = prefixLength === 0 ? rootId() : idsBeforeChange[prefixLength - 1]
    if (!parentId) {
      requestFullSync()
      statusMessage.value = 'Local edit parent was missing; requested full sync'
      return false
    }

    for (let index = 0; index < insertText.length; index += 1) {
      const operation = engine.localInsert(insertText[index] ?? '', parentId)
      if (isErrorMessage(operation)) {
        requestFullSync()
        statusMessage.value = operation.message
        return false
      }

      sendToServer(operation)
      parentId = operation.id
    }

    editorText.value = engine.toString()
    statusMessage.value = isConnected.value ? 'Editing with CRDT sync' : 'Editing locally'
    return true
  }

  function updateText(value: string) {
    const applied = applyLocalTextChange(value)
    if (!applied) {
      return
    }

    recordHistory(editorText.value)
  }

  function undoEdit() {
    if (!canUndo.value) {
      return
    }

    isApplyingHistory = true
    historyIndex.value -= 1
    const targetText = history.value[historyIndex.value] ?? editorText.value
    const applied = applyLocalTextChange(targetText)
    isApplyingHistory = false

    if (applied) {
      statusMessage.value = 'Undo applied'
    }
  }

  function redoEdit() {
    if (!canRedo.value) {
      return
    }

    isApplyingHistory = true
    historyIndex.value += 1
    const targetText = history.value[historyIndex.value] ?? editorText.value
    const applied = applyLocalTextChange(targetText)
    isApplyingHistory = false

    if (applied) {
      statusMessage.value = 'Redo applied'
    }
  }

  function applySyncState(message: SyncStateMessage) {
    if (!engine) {
      return
    }

    engine.loadSnapshot(message.nodes)
    editorText.value = engine.toString()
    recordHistory(editorText.value)
    hasUnsavedChanges.value = false
    lastSavedAt.value = nowLabel()
    statusMessage.value = 'Updated from server snapshot'
  }

  function applyRemoteInsert(message: InsertMessage) {
    if (!engine) {
      return
    }

    const applied = engine.applyRemoteInsert(message)
    if (!applied) {
      requestFullSync()
      statusMessage.value = 'Remote insert referenced missing state; requested full sync'
      return
    }

    editorText.value = engine.toString()
    recordHistory(editorText.value)
    statusMessage.value = 'Remote insert applied'
  }

  function applyRemoteDelete(message: DeleteMessage) {
    if (!engine) {
      return
    }

    const applied = engine.applyRemoteDelete(message)
    if (!applied) {
      requestFullSync()
      statusMessage.value = 'Remote delete referenced missing state; requested full sync'
      return
    }

    editorText.value = engine.toString()
    recordHistory(editorText.value)
    statusMessage.value = 'Remote delete applied'
  }

  function handleServerMessage(event: MessageEvent<string>) {
    let message: ServerMessage

    try {
      message = JSON.parse(event.data) as ServerMessage
    } catch {
      statusMessage.value = 'Ignored invalid server JSON'
      return
    }

    switch (message.type) {
      case 'assign_peer_id':
        peerId.value = message.peer_id
        engine?.setPeerId(message.peer_id)
        statusMessage.value = `Assigned peer ${message.peer_id}`
        break

      case 'sync_state':
        applySyncState(message)
        break

      case 'remote_insert':
        applyRemoteInsert(message)
        break

      case 'remote_delete':
        applyRemoteDelete(message)
        break

      case 'error':
        statusMessage.value = message.message ?? 'Server reported an error'
        requestFullSync()
        break

      default:
        statusMessage.value = 'Ignored unsupported server message'
        break
    }
  }

  function disconnectSocket() {
    if (!socket) {
      return
    }

    socket.onopen = null
    socket.onmessage = null
    socket.onerror = null
    socket.onclose = null
    socket.close()
    socket = null
  }

  async function connectSocket() {
    const ready = await initializeEngine()
    if (!ready) {
      socketState.value = 'disconnected'
      return
    }

    disconnectSocket()
    socketState.value = 'connecting'
    statusMessage.value = 'Connecting to server'

    try {
      socket = new WebSocket(serverUrl.value)
    } catch {
      socketState.value = 'disconnected'
      statusMessage.value = 'Invalid server URL'
      return
    }

    socket.onopen = () => {
      socketState.value = 'connected'
      statusMessage.value = 'Connected to server'
      requestFullSync()
    }

    socket.onmessage = handleServerMessage

    socket.onerror = () => {
      socketState.value = 'disconnected'
      statusMessage.value = 'Could not reach server'
    }

    socket.onclose = () => {
      socketState.value = 'disconnected'
      statusMessage.value = 'Disconnected'
      socket = null
    }
  }

  function handleShortcut(event: KeyboardEvent) {
    const usesModifier = event.metaKey || event.ctrlKey

    if (!usesModifier) {
      return
    }

    if (event.key.toLowerCase() === 's') {
      event.preventDefault()
      saveDocument()
      return
    }

    if (event.key.toLowerCase() !== 'z') {
      return
    }

    event.preventDefault()

    if (event.shiftKey) {
      redoEdit()
      return
    }

    undoEdit()
  }

  onMounted(async () => {
    await connectSocket()

    if (isDestroyed) {
      disconnectSocket()
      return
    }

    window.addEventListener('keydown', handleShortcut)
  })

  onBeforeUnmount(() => {
    isDestroyed = true
    window.removeEventListener('keydown', handleShortcut)
    disconnectSocket()
    engine?.destroy()
    engine = null
  })

  return {
    canRedo,
    canUndo,
    connectionLabel,
    editorText,
    saveDocument,
    saveLabel,
    serverUrl,
    socketState,
    statusMessage,
    connectSocket,
    redoEdit,
    undoEdit,
    updateText,
  }
}
