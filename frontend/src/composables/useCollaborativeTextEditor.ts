import { computed, onBeforeUnmount, onMounted, ref } from 'vue'

export type SocketState = 'connecting' | 'connected' | 'disconnected'

type ServerMessage = {
  type?: string
  content?: string
}

const DEFAULT_DOCUMENT_PATH = '/shared.txt'
const SYNC_INTERVAL_MS = 3000

const defaultText = [
  'Start writing your shared text file here.',
  '',
  'This editor is plain text only for now. Formatting, CRDT merging, and rich text features will be handled later by the backend and future editor work.',
].join('\n')

export function useCollaborativeTextEditor() {
  const editorText = ref(defaultText)
  const serverUrl = ref('ws://localhost:8080/ws')
  const socketState = ref<SocketState>('disconnected')
  const statusMessage = ref('Disconnected')
  const hasUnsavedChanges = ref(false)
  const lastSavedAt = ref<string | null>(null)

  const history = ref<string[]>([defaultText])
  const historyIndex = ref(0)

  let socket: WebSocket | null = null
  let syncTimer: number | undefined
  let isApplyingHistory = false

  const canUndo = computed(() => historyIndex.value > 0)
  const canRedo = computed(() => historyIndex.value < history.value.length - 1)
  const isConnected = computed(() => socketState.value === 'connected')

  const saveLabel = computed(() => {
    if (hasUnsavedChanges.value) {
      return 'Unsaved changes'
    }

    if (lastSavedAt.value) {
      return `Saved ${lastSavedAt.value}`
    }

    return 'Saved locally'
  })

  const connectionLabel = computed(() => {
    if (socketState.value === 'connected') {
      return 'Connected'
    }

    if (socketState.value === 'connecting') {
      return 'Connecting'
    }

    return 'Disconnected'
  })

  function nowLabel() {
    return new Intl.DateTimeFormat(undefined, {
      hour: '2-digit',
      minute: '2-digit',
    }).format(new Date())
  }

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

  function updateText(value: string) {
    editorText.value = value
    hasUnsavedChanges.value = true
    statusMessage.value = 'Editing locally'
    recordHistory(value)
  }

  function undoEdit() {
    if (!canUndo.value) {
      return
    }

    isApplyingHistory = true
    historyIndex.value -= 1
    editorText.value = history.value[historyIndex.value] ?? editorText.value
    isApplyingHistory = false
    hasUnsavedChanges.value = true
    statusMessage.value = 'Undo applied'
  }

  function redoEdit() {
    if (!canRedo.value) {
      return
    }

    isApplyingHistory = true
    historyIndex.value += 1
    editorText.value = history.value[historyIndex.value] ?? editorText.value
    isApplyingHistory = false
    hasUnsavedChanges.value = true
    statusMessage.value = 'Redo applied'
  }

  function sendToServer(payload: Record<string, unknown>) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      return false
    }

    socket.send(JSON.stringify(payload))
    return true
  }

  function requestLatestText() {
    sendToServer({
      type: 'text:request',
      documentPath: DEFAULT_DOCUMENT_PATH,
    })
  }

  function saveDocument() {
    const sent = sendToServer({
      type: 'text:update',
      documentPath: DEFAULT_DOCUMENT_PATH,
      content: editorText.value,
    })

    hasUnsavedChanges.value = false
    lastSavedAt.value = nowLabel()
    statusMessage.value = sent ? 'Saved to server' : 'Saved locally'
  }

  function applyServerSnapshot(content: string) {
    if (content === editorText.value) {
      hasUnsavedChanges.value = false
      statusMessage.value = 'Already up to date'
      return
    }

    editorText.value = content
    recordHistory(content)
    hasUnsavedChanges.value = false
    statusMessage.value = 'Updated from server'
  }

  function handleServerMessage(event: MessageEvent<string>) {
    try {
      const message = JSON.parse(event.data) as ServerMessage

      if (message.type === 'text:snapshot' && typeof message.content === 'string') {
        applyServerSnapshot(message.content)
      }
    } catch {
      statusMessage.value = 'Ignored unsupported server message'
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

  function connectSocket() {
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
      sendToServer({
        type: 'join',
        documentPath: DEFAULT_DOCUMENT_PATH,
      })
      requestLatestText()
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

  function syncWithServer() {
    if (!isConnected.value) {
      return
    }

    if (hasUnsavedChanges.value) {
      saveDocument()
    }

    requestLatestText()
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

  onMounted(() => {
    connectSocket()
    window.addEventListener('keydown', handleShortcut)
    syncTimer = window.setInterval(syncWithServer, SYNC_INTERVAL_MS)
  })

  onBeforeUnmount(() => {
    window.removeEventListener('keydown', handleShortcut)

    if (syncTimer) {
      window.clearInterval(syncTimer)
    }

    disconnectSocket()
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
