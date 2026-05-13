import { computed, onBeforeUnmount, onMounted, ref } from 'vue'

export type SocketState = 'connecting' | 'connected' | 'disconnected'

export type RemoteCharacter = {
  id: string
  value: string
}

export type RemoteUser = {
  clientId: string
  userName: string
  cursor: {
    afterId: string
  }
}

type ServerMessage = {
  type?: string
  revision?: number
  text?: string
  chars?: RemoteCharacter[]
  fontSize?: number
  users?: RemoteUser[]
}

type EditOperation =
  | {
      type: 'delete'
      ids: string[]
    }
  | {
      type: 'insert'
      afterId: string
      text: string
    }

const CLIENT_ID_KEY = 'nettverk-editor-client-id'
const USER_NAME_KEY = 'nettverk-editor-user-name'
const DEFAULT_DOCUMENT_PATH = '/shared.txt'
const SYNC_INTERVAL_MS = 5000

function createClientId() {
  return `client-${crypto.randomUUID()}`
}

function storedValue(key: string, fallback: string) {
  const value = window.localStorage.getItem(key)
  return value && value.trim().length > 0 ? value : fallback
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
  const max = Math.min(left.length, right.length) - prefixLength
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
  const editorText = ref('')
  const serverUrl = ref('ws://localhost:3030/ws')
  const userName = ref('Anonymous')
  const clientId = ref('')
  const socketState = ref<SocketState>('disconnected')
  const statusMessage = ref('Disconnected')
  const hasUnsavedChanges = ref(false)
  const isWaitingForSnapshot = ref(false)
  const lastSavedAt = ref<string | null>(null)
  const baseRevision = ref(0)
  const baseChars = ref<RemoteCharacter[]>([])
  const fontSize = ref(16)
  const remoteUsers = ref<RemoteUser[]>([])
  const cursorIndex = ref(0)

  const history = ref<string[]>([''])
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

    if (isWaitingForSnapshot.value) {
      return 'Waiting for server'
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

  const visibleRemoteUsers = computed(() =>
    remoteUsers.value.filter((user) => user.clientId !== clientId.value),
  )

  function currentCursorAfterId() {
    if (cursorIndex.value <= 0) {
      return ''
    }

    const character = baseChars.value[Math.min(cursorIndex.value, baseChars.value.length) - 1]
    return character?.id ?? ''
  }

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

  function buildOperations(nextText: string): EditOperation[] {
    const previousText = baseChars.value.map((character) => character.value).join('')

    if (previousText === nextText) {
      return []
    }

    const prefixLength = commonPrefixLength(previousText, nextText)
    const suffixLength = commonSuffixLength(previousText, nextText, prefixLength)

    const deletedIds = baseChars.value
      .slice(prefixLength, previousText.length - suffixLength)
      .map((character) => character.id)

    const insertedText = nextText.slice(prefixLength, nextText.length - suffixLength)
    const afterId = prefixLength === 0 ? '' : baseChars.value[prefixLength - 1]?.id ?? ''

    const ops: EditOperation[] = []

    if (deletedIds.length > 0) {
      ops.push({ type: 'delete', ids: deletedIds })
    }

    if (insertedText.length > 0) {
      ops.push({ type: 'insert', afterId, text: insertedText })
    }

    return ops
  }

  function updateText(value: string) {
    editorText.value = value
    hasUnsavedChanges.value = true
    statusMessage.value = 'Editing locally'
    recordHistory(value)
  }

  function updateCursor(index: number) {
    cursorIndex.value = index

    sendToServer({
      type: 'cursor:update',
      clientId: clientId.value,
      userName: userName.value,
      cursor: { afterId: currentCursorAfterId() },
    })
  }

  function updateUserName(value: string) {
    userName.value = value || 'Anonymous'
    window.localStorage.setItem(USER_NAME_KEY, userName.value)
    updateCursor(cursorIndex.value)
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
      type: 'document:request',
      documentPath: DEFAULT_DOCUMENT_PATH,
      clientId: clientId.value,
      userName: userName.value,
      cursor: { afterId: currentCursorAfterId() },
    })
  }

  function saveDocument() {
    if (isWaitingForSnapshot.value) {
      return
    }

    const ops = buildOperations(editorText.value)

    if (ops.length === 0) {
      hasUnsavedChanges.value = false
      statusMessage.value = 'Already up to date'
      requestLatestText()
      return
    }

    const sent = sendToServer({
      type: 'edit:batch',
      documentPath: DEFAULT_DOCUMENT_PATH,
      clientId: clientId.value,
      userName: userName.value,
      baseRevision: baseRevision.value,
      ops,
      cursor: { afterId: currentCursorAfterId() },
    })

    if (sent) {
      isWaitingForSnapshot.value = true
      hasUnsavedChanges.value = false
      lastSavedAt.value = nowLabel()
      statusMessage.value = 'Sent changes to server'
      return
    }

    statusMessage.value = 'Saved locally'
  }

  function setFontSize(value: number) {
    fontSize.value = Math.min(48, Math.max(10, value))

    sendToServer({
      type: 'font-size:set',
      documentPath: DEFAULT_DOCUMENT_PATH,
      clientId: clientId.value,
      userName: userName.value,
      value: fontSize.value,
      cursor: { afterId: currentCursorAfterId() },
    })
  }

  function applyServerSnapshot(message: ServerMessage) {
    if (!Array.isArray(message.chars)) {
      return
    }

    baseRevision.value = message.revision ?? baseRevision.value
    baseChars.value = message.chars
    fontSize.value = message.fontSize ?? fontSize.value
    remoteUsers.value = Array.isArray(message.users) ? message.users : []
    isWaitingForSnapshot.value = false

    if (!hasUnsavedChanges.value) {
      editorText.value = message.text ?? baseChars.value.map((character) => character.value).join('')
      recordHistory(editorText.value)
      statusMessage.value = 'Updated from server'
    } else {
      statusMessage.value = 'Server updated; local edits pending'
    }
  }

  function handleServerMessage(event: MessageEvent<string>) {
    try {
      const message = JSON.parse(event.data) as ServerMessage

      if (message.type === 'document:snapshot') {
        applyServerSnapshot(message)
      } else if (message.type === 'error') {
        statusMessage.value = 'Server rejected a message'
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
        clientId: clientId.value,
        userName: userName.value,
        cursor: { afterId: currentCursorAfterId() },
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
    } else {
      requestLatestText()
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

  onMounted(() => {
    clientId.value = storedValue(CLIENT_ID_KEY, createClientId())
    userName.value = storedValue(USER_NAME_KEY, 'Anonymous')
    window.localStorage.setItem(CLIENT_ID_KEY, clientId.value)
    window.localStorage.setItem(USER_NAME_KEY, userName.value)

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
    baseChars,
    baseRevision,
    canRedo,
    canUndo,
    clientId,
    connectionLabel,
    editorText,
    fontSize,
    saveDocument,
    saveLabel,
    serverUrl,
    socketState,
    statusMessage,
    userName,
    visibleRemoteUsers,
    connectSocket,
    redoEdit,
    setFontSize,
    undoEdit,
    updateCursor,
    updateText,
    updateUserName,
  }
}
