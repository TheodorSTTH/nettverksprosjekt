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

import { useLWWRegister, type LWWMetadata } from './useLWWRegister'

export type SocketState = 'connecting' | 'connected' | 'disconnected'

export type TextSelection = {
  start: number
  end: number
}

export type SelectionRestore = TextSelection & {
  revision: number
}

export type RemoteEditorCursor = {
  peerId: number
  label: string
  color: string
  cursorIndex: number
}

type CursorAnchor = {
  anchorId: RgaId | null
  cursorIndex: number
}

type TextSegment = {
  text: string
  start: number
  end: number
}

type AssignPeerMessage = {
  type: 'assign_peer_id'
  peer_id: number
}

type SyncStateMessage = {
  type: 'sync_state'
  nodes: RgaNode[]
  visible_text?: string
  title?: { value: string; timestamp: number; peer_id: number }
}

type SetTitleMessage = {
  type: 'set_title'
  value: string
  timestamp: number
  peer_id: number
}

type ServerErrorMessage = {
  type: 'error'
  message?: string
}

type RemoteCursorMessage = {
  type: 'remote_cursor'
  peer_id: number
  anchor_id: RgaId | null
  cursor_index: number
}

type PeerLeftMessage = {
  type: 'peer_left'
  peer_id: number
}

type CounterStateMessage = {
  type: 'counter_state'
  counter: 'cookies'
  value: number
  own_count: number
  replica_id?: string
}

type ServerMessage =
  | AssignPeerMessage
  | SyncStateMessage
  | InsertMessage
  | DeleteMessage
  | RemoteCursorMessage
  | PeerLeftMessage
  | SetTitleMessage
  | CounterStateMessage
  | ServerErrorMessage

const defaultText = ''
const defaultTitle = 'Untitled'
const defaultSelection: TextSelection = { start: 0, end: 0 }
const cursorUpdateDelayMs = 100
const peerColors = ['#2563eb', '#dc2626', '#16a34a', '#9333ea', '#ea580c', '#0891b2']
const PENDING_OPS_STORAGE_KEY = 'cookiedoc:pendingOps:v1'
const COOKIE_REPLICA_ID_STORAGE_KEY = 'cookiedoc:cookieReplicaId:v1'
const reconnectInitialDelayMs = 1000
const reconnectMaxDelayMs = 15000

// Random fallback peer id used for local LWW writes until the server assigns
// one via `assign_peer_id`. Once assigned, the LWW register's peer id is
// updated so subsequent writes use the canonical id.
const fallbackPeerId = Math.floor(Math.random() * 0xffffffff) + 1

function nowLabel() {
  return new Intl.DateTimeFormat(undefined, {
    hour: '2-digit',
    minute: '2-digit',
  }).format(new Date())
}

function textSegments(value: string): TextSegment[] {
  const segmenter =
    typeof Intl !== 'undefined' && 'Segmenter' in Intl
      ? new Intl.Segmenter(undefined, { granularity: 'grapheme' })
      : null

  if (segmenter) {
    return Array.from(segmenter.segment(value), (segment) => ({
      text: segment.segment,
      start: segment.index,
      end: segment.index + segment.segment.length,
    }))
  }

  const segments: TextSegment[] = []
  let offset = 0
  for (const text of Array.from(value)) {
    segments.push({ text, start: offset, end: offset + text.length })
    offset += text.length
  }
  return segments
}

function graphemeIndexFromOffset(value: string, offset: number) {
  const clampedOffset = Math.max(0, Math.min(offset, value.length))
  const segments = textSegments(value)

  for (let index = 0; index < segments.length; index += 1) {
    const segment = segments[index]
    if (!segment) {
      continue
    }

    if (clampedOffset <= segment.start) {
      return index
    }

    if (clampedOffset <= segment.end) {
      return index + 1
    }
  }

  return segments.length
}

function offsetFromGraphemeIndex(value: string, graphemeIndex: number) {
  const segments = textSegments(value)
  const clampedIndex = Math.max(0, Math.min(graphemeIndex, segments.length))
  if (clampedIndex === segments.length) {
    return value.length
  }

  return segments[clampedIndex]?.start ?? value.length
}

function commonPrefixLength(left: TextSegment[], right: TextSegment[]) {
  const max = Math.min(left.length, right.length)
  let index = 0

  while (index < max && left[index]?.text === right[index]?.text) {
    index += 1
  }

  return index
}

function commonSuffixLength(left: TextSegment[], right: TextSegment[], prefixLength: number) {
  const max = Math.min(left.length - prefixLength, right.length - prefixLength)
  let count = 0

  while (
    count < max &&
    left[left.length - 1 - count]?.text === right[right.length - 1 - count]?.text
  ) {
    count += 1
  }

  return count
}

function createCookieReplicaId() {
  if (typeof crypto !== 'undefined' && 'randomUUID' in crypto) {
    return `cookie-${crypto.randomUUID()}`
  }

  return `cookie-${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`
}

export function useCollaborativeTextEditor() {
  const editorText = ref(defaultText)
  const serverUrl = ref('ws://localhost:3030/ws')
  const socketState = ref<SocketState>('disconnected')
  const isManuallyOffline = ref(false)
  const statusMessage = ref('Loading CRDT WebAssembly')
  const hasUnsavedChanges = ref(false)
  const lastSavedAt = ref<string | null>(null)
  const peerId = ref<number | null>(null)
  const wasmReady = ref(false)
  const currentSelection = ref<TextSelection>(defaultSelection)
  const selectionRestore = ref<SelectionRestore | null>(null)
  const remoteCursorAnchors = ref<Record<number, CursorAnchor>>({})
  const cookieServerCount = ref(0)
  const cookieConfirmedOwnCount = ref(0)
  const cookieOwnCount = ref(0)
  const cookieReplicaId = loadCookieReplicaId()

  // LWW-Register for the document title. Local writes happen only on commit
  // (blur/Enter from DocumentTitle.vue); remote writes resolve via LWW order.
  const titleRegister = useLWWRegister<string>(
    () => peerId.value ?? fallbackPeerId,
    defaultTitle,
  )

  const history = ref<string[]>([defaultText])
  const historyIndex = ref(0)
  const pendingOps = ref<Record<string, unknown>[]>(loadPendingOps())
  let selectionRestoreRevision = 0
  let cursorUpdateTimer: ReturnType<typeof setTimeout> | null = null
  let lastSentCursorUpdate = ''
  let reconnectAttempt = 0
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null

  let socket: WebSocket | null = null
  let engine: CrdtEngine | null = null
  let isApplyingHistory = false
  let isDestroyed = false

  function loadPendingOps(): Record<string, unknown>[] {
    try {
      const raw = window.localStorage.getItem(PENDING_OPS_STORAGE_KEY)
      if (!raw) return []
      const parsed = JSON.parse(raw)
      return Array.isArray(parsed) ? parsed : []
    } catch {
      return []
    }
  }

  function loadCookieReplicaId() {
    try {
      const existing = window.localStorage.getItem(COOKIE_REPLICA_ID_STORAGE_KEY)
      if (existing) {
        return existing
      }

      const created = createCookieReplicaId()
      window.localStorage.setItem(COOKIE_REPLICA_ID_STORAGE_KEY, created)
      return created
    } catch {
      return createCookieReplicaId()
    }
  }

  function persistPendingOps() {
    try {
      window.localStorage.setItem(
        PENDING_OPS_STORAGE_KEY,
        JSON.stringify(pendingOps.value),
      )
    } catch {
      // Quota exceeded or private mode — best effort only.
    }
  }

  const canUndo = computed(() => historyIndex.value > 0)
  const canRedo = computed(() => historyIndex.value < history.value.length - 1)
  const isConnected = computed(() => socketState.value === 'connected')
  const cookieDisplayCount = computed(() => (
    cookieServerCount.value - cookieConfirmedOwnCount.value + cookieOwnCount.value
  ))
  const remoteCursors = computed<RemoteEditorCursor[]>(() => {
    // Track editor text so this recomputes whenever CRDT visible order changes.
    editorText.value
    const ids = visibleIds()

    return Object.entries(remoteCursorAnchors.value)
      .map(([peerId, anchor]) => {
        const numericPeerId = Number(peerId)
        const cursorIndex = offsetFromGraphemeIndex(editorText.value, resolveCursorIndex(anchor as CursorAnchor, ids))

        return {
          peerId: numericPeerId,
          label: `Peer ${numericPeerId}`,
          color: peerColor(numericPeerId),
          cursorIndex,
        }
      })
      .sort((left, right) => left.peerId - right.peerId)
  })

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
      // Cursor updates are ephemeral; replaying a stale position after
      // reconnect makes no sense, so we drop them. Everything else queues.
      if (payload.type !== 'cursor_update') {
        pendingOps.value.push(payload)
        persistPendingOps()
        statusMessage.value =
          `Editing offline; ${pendingOps.value.length} change(s) queued`
      }
      hasUnsavedChanges.value = true
      return false
    }

    socket.send(JSON.stringify(payload))
    lastSavedAt.value = nowLabel()
    hasUnsavedChanges.value = pendingOps.value.length > 0
    return true
  }

  function flushPendingOps() {
    if (!socket || socket.readyState !== WebSocket.OPEN) return
    if (pendingOps.value.length === 0) return

    // Collapse counter_update messages — server's G-Counter merge keeps the
    // max per stable replica, so replaying every intermediate value is redundant.
    const collapsed: Record<string, unknown>[] = []
    let cookieIndex = -1
    for (const op of pendingOps.value) {
      if (op.type === 'counter_update') {
        const cookieUpdate = {
          ...op,
          replica_id: cookieReplicaId,
        }

        if (cookieIndex >= 0) {
          collapsed[cookieIndex] = cookieUpdate
        } else {
          collapsed.push(cookieUpdate)
          cookieIndex = collapsed.length - 1
        }
      } else {
        collapsed.push(op)
      }
    }

    for (const op of collapsed) {
      socket.send(JSON.stringify(op))
    }

    const count = pendingOps.value.length
    pendingOps.value = []
    persistPendingOps()
    hasUnsavedChanges.value = false
    lastSavedAt.value = nowLabel()
    statusMessage.value = `Reconnected; synced ${count} offline change(s)`
  }

  function requestFullSync() {
    sendToServer({ type: 'sync_request' })
  }

  function saveDocument() {
    const sent = sendToServer({ type: 'save_request' })
    statusMessage.value = sent ? 'Saving after pending updates' : 'Reconnect to sync'
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

  function sameId(left: RgaId, right: RgaId) {
    return left.peer_id === right.peer_id && left.sequence === right.sequence
  }

  function clampCursorIndex(index: number, max: number) {
    return Math.max(0, Math.min(index, max))
  }

  function peerColor(remotePeerId: number) {
    return peerColors[remotePeerId % peerColors.length] ?? '#2563eb'
  }

  function cursorAnchorFromSelection(selection = currentSelection.value): CursorAnchor {
    const ids = visibleIds()
    const cursorIndex = clampCursorIndex(
      graphemeIndexFromOffset(editorText.value, selection.start),
      ids.length,
    )
    const anchorId = cursorIndex === 0 ? null : ids[cursorIndex - 1] ?? null

    return { anchorId, cursorIndex }
  }

  function normalizeCursorAnchor(anchor: CursorAnchor): CursorAnchor {
    if (!anchor.anchorId || !engine) {
      return {
        anchorId: null,
        cursorIndex: 0,
      }
    }

    const nodes = engine.nodesJson()
    const anchorIndex = nodes.findIndex((node) => sameId(node.id, anchor.anchorId as RgaId))

    if (anchorIndex === -1) {
      const ids = visibleIds()
      return {
        anchorId: anchor.anchorId,
        cursorIndex: clampCursorIndex(anchor.cursorIndex, ids.length),
      }
    }

    if (!nodes[anchorIndex]?.tombstoned) {
      const visibleIndex = nodes
        .slice(0, anchorIndex + 1)
        .filter((node) => !node.tombstoned)
        .length

      return {
        anchorId: anchor.anchorId,
        cursorIndex: visibleIndex,
      }
    }

    for (let index = anchorIndex - 1; index >= 0; index -= 1) {
      const node = nodes[index]
      if (node && !node.tombstoned) {
        const visibleIndex = nodes
          .slice(0, index + 1)
          .filter((candidate) => !candidate.tombstoned)
          .length

        return {
          anchorId: node.id,
          cursorIndex: visibleIndex,
        }
      }
    }

    return {
      anchorId: null,
      cursorIndex: 0,
    }
  }

  function normalizeRemoteCursorAnchors() {
    const normalizedEntries = Object.entries(remoteCursorAnchors.value).map(([remotePeerId, anchor]) => [
      remotePeerId,
      normalizeCursorAnchor(anchor as CursorAnchor),
    ])

    remoteCursorAnchors.value = Object.fromEntries(normalizedEntries)
  }

  function resolveCursorIndex(anchor: CursorAnchor, ids = visibleIds()) {
    const normalized = normalizeCursorAnchor(anchor)

    if (!normalized.anchorId) {
      return clampCursorIndex(normalized.cursorIndex, ids.length)
    }

    const anchoredIndex = ids.findIndex((id) => sameId(id, normalized.anchorId as RgaId))
    if (anchoredIndex !== -1) {
      return anchoredIndex + 1
    }

    return clampCursorIndex(normalized.cursorIndex, ids.length)
  }

  function captureCursorAnchor() {
    const ids = visibleIds()
    const cursorIndex = clampCursorIndex(
      graphemeIndexFromOffset(editorText.value, currentSelection.value.start),
      ids.length,
    )

    return {
      fallbackIds: ids.slice(0, cursorIndex).reverse(),
    }
  }

  function restoreCursorFromAnchor(anchor: { fallbackIds: RgaId[] }) {
    const ids = visibleIds()
    let cursorIndex = 0

    for (const fallbackId of anchor.fallbackIds) {
      const index = ids.findIndex((id) => sameId(id, fallbackId))
      if (index !== -1) {
        cursorIndex = index + 1
        break
      }
    }

    const offset = offsetFromGraphemeIndex(editorText.value, cursorIndex)
    const selection = { start: offset, end: offset }
    currentSelection.value = selection
    selectionRestore.value = {
      ...selection,
      revision: ++selectionRestoreRevision,
    }
  }

  function updateSelection(selection: TextSelection) {
    currentSelection.value = {
      start: Math.max(0, selection.start),
      end: Math.max(0, selection.end),
    }
    scheduleCursorUpdate()
  }

  function sendCursorUpdateNow() {
    cursorUpdateTimer = null
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      return
    }

    const anchor = cursorAnchorFromSelection()
    const payload = {
      type: 'cursor_update',
      anchor_id: anchor.anchorId,
      cursor_index: anchor.cursorIndex,
    }
    const serialized = JSON.stringify(payload)
    if (serialized === lastSentCursorUpdate) {
      return
    }

    socket.send(serialized)
    lastSentCursorUpdate = serialized
  }

  function scheduleCursorUpdate() {
    if (cursorUpdateTimer) {
      return
    }

    cursorUpdateTimer = window.setTimeout(sendCursorUpdateNow, cursorUpdateDelayMs)
  }

  function applyRemoteCursor(message: RemoteCursorMessage) {
    if (message.peer_id === peerId.value) {
      return
    }

    remoteCursorAnchors.value = {
      ...remoteCursorAnchors.value,
      [message.peer_id]: normalizeCursorAnchor({
        anchorId: message.anchor_id,
        cursorIndex: Math.max(0, message.cursor_index),
      }),
    }
  }

  function removeRemoteCursor(peerIdToRemove: number) {
    const next = { ...remoteCursorAnchors.value }
    delete next[peerIdToRemove]
    remoteCursorAnchors.value = next
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
    const previousSegments = textSegments(previousText)
    const nextSegments = textSegments(nextText)
    const prefixLength = commonPrefixLength(previousSegments, nextSegments)
    const suffixLength = commonSuffixLength(previousSegments, nextSegments, prefixLength)
    const deleteCount = previousSegments.length - prefixLength - suffixLength
    const insertedSegments = nextSegments.slice(prefixLength, nextSegments.length - suffixLength)

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

    for (const segment of insertedSegments) {
      const operation = engine.localInsert(segment.text, parentId)
      if (isErrorMessage(operation)) {
        requestFullSync()
        statusMessage.value = operation.message
        return false
      }

      sendToServer(operation)
      parentId = operation.id
    }

    editorText.value = engine.toString()
    normalizeRemoteCursorAnchors()
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

  function clickCookie() {
    const nextCount = cookieOwnCount.value + 1
    cookieOwnCount.value = nextCount
    sendToServer({
      type: 'counter_update',
      counter: 'cookies',
      replica_id: cookieReplicaId,
      count: nextCount,
    })
    if (isConnected.value) {
      statusMessage.value = `Cookies: ${cookieDisplayCount.value}`
    }
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

  // Called by DocumentTitle.vue on commit (blur/Enter). Apply locally first so
  // the UI reflects the change immediately, then broadcast the LWW op. If the
  // server later returns a winning op with a higher timestamp, apply() will
  // overwrite ours — the CRDT guarantees both replicas converge.
  function commitTitle(newTitle: string) {
    const op = titleRegister.set(newTitle)
    sendToServer({
      type: 'set_title',
      value: op.value,
      timestamp: op.metadata.timestamp,
      peer_id: op.metadata.peerId,
    })
  }

  function applyRemoteTitle(meta: LWWMetadata, value: string) {
    titleRegister.apply({ value, metadata: meta })
  }

  function applySyncState(message: SyncStateMessage) {
    if (!engine) {
      return
    }

    const cursorAnchor = captureCursorAnchor()

    // If the local engine already has state (typical on reconnect or when
    // offline edits exist), do NOT call loadSnapshot — it would wipe local
    // state. Instead, replay each snapshot node as a remote op. The CRDT is
    // idempotent: known IDs are ignored, unknown ones get integrated. This
    // converges both replicas without flicker or data loss.
    const hasLocalState =
      engine.visibleIds().length > 0 || pendingOps.value.length > 0

    if (hasLocalState) {
      for (const node of message.nodes) {
        engine.applyRemoteInsert({
          type: 'remote_insert',
          value: node.value,
          char: node.value,
          id: node.id,
          parent_id: node.parent_id,
        })
        if (node.tombstoned) {
          engine.applyRemoteDelete({
            type: 'remote_delete',
            target_id: node.id,
          })
        }
      }
    } else {
      engine.loadSnapshot(message.nodes)
    }

    editorText.value = engine.toString()
    normalizeRemoteCursorAnchors()
    restoreCursorFromAnchor(cursorAnchor)
    recordHistory(editorText.value)
    hasUnsavedChanges.value = pendingOps.value.length > 0
    lastSavedAt.value = nowLabel()
    statusMessage.value = hasLocalState
      ? 'Merged server snapshot with local state'
      : 'Updated from server snapshot'

    if (message.title) {
      applyRemoteTitle(
        { timestamp: message.title.timestamp, peerId: message.title.peer_id },
        message.title.value,
      )
    }
  }

  function applyRemoteInsert(message: InsertMessage) {
    if (!engine) {
      return
    }

    const cursorAnchor = captureCursorAnchor()
    const applied = engine.applyRemoteInsert(message)
    if (!applied) {
      requestFullSync()
      statusMessage.value = 'Remote insert referenced missing state; requested full sync'
      return
    }

    editorText.value = engine.toString()
    normalizeRemoteCursorAnchors()
    restoreCursorFromAnchor(cursorAnchor)
    recordHistory(editorText.value)
    statusMessage.value = 'Remote insert applied'
  }

  function applyRemoteDelete(message: DeleteMessage) {
    if (!engine) {
      return
    }

    const cursorAnchor = captureCursorAnchor()
    const applied = engine.applyRemoteDelete(message)
    if (!applied) {
      requestFullSync()
      statusMessage.value = 'Remote delete referenced missing state; requested full sync'
      return
    }

    editorText.value = engine.toString()
    normalizeRemoteCursorAnchors()
    restoreCursorFromAnchor(cursorAnchor)
    recordHistory(editorText.value)
    statusMessage.value = 'Remote delete applied'
  }

  function applyCounterState(message: CounterStateMessage) {
    if (message.counter !== 'cookies') {
      return
    }

    if (message.replica_id && message.replica_id !== cookieReplicaId) {
      return
    }

    cookieServerCount.value = Math.max(0, message.value)
    cookieConfirmedOwnCount.value = Math.max(0, message.own_count)
    cookieOwnCount.value = Math.max(cookieOwnCount.value, cookieConfirmedOwnCount.value)
  }

  function requestCookieCounterState() {
    sendToServer({
      type: 'counter_state_request',
      counter: 'cookies',
      replica_id: cookieReplicaId,
    })
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

      case 'remote_cursor':
        applyRemoteCursor(message)
        break

      case 'peer_left':
        removeRemoteCursor(message.peer_id)
        break

      case 'set_title':
        applyRemoteTitle(
          { timestamp: message.timestamp, peerId: message.peer_id },
          message.value,
        )
        break

      case 'counter_state':
        applyCounterState(message)
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

  function clearReconnectTimer() {
    if (reconnectTimer !== null) {
      window.clearTimeout(reconnectTimer)
      reconnectTimer = null
    }
  }

  function scheduleReconnect() {
    if (isDestroyed) return
    if (isManuallyOffline.value) {
      statusMessage.value =
        'Manually offline' +
        (pendingOps.value.length > 0
          ? ` — ${pendingOps.value.length} change(s) queued`
          : '')
      return
    }
    clearReconnectTimer()
    const delay = Math.min(
      reconnectInitialDelayMs * 2 ** reconnectAttempt,
      reconnectMaxDelayMs,
    )
    reconnectAttempt += 1
    statusMessage.value =
      `Disconnected; retrying in ${Math.round(delay / 1000)}s` +
      (pendingOps.value.length > 0
        ? ` (${pendingOps.value.length} queued)`
        : '')
    reconnectTimer = window.setTimeout(() => {
      reconnectTimer = null
      void connectSocket()
    }, delay)
  }

  function disconnectSocket() {
    clearReconnectTimer()
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
      scheduleReconnect()
      return
    }

    socket.onopen = () => {
      socketState.value = 'connected'
      statusMessage.value = 'Connected to server'
      lastSentCursorUpdate = ''
      reconnectAttempt = 0
      // Ask for this browser's stored counter component before flushing queued
      // absolute counter updates for the same stable replica ID.
      requestCookieCounterState()
      flushPendingOps()
      scheduleCursorUpdate()
    }

    socket.onmessage = handleServerMessage

    socket.onerror = () => {
      // onerror is always followed by onclose; defer reconnect scheduling
      // to onclose so we don't double-schedule.
      statusMessage.value = 'Could not reach server'
    }

    socket.onclose = () => {
      socketState.value = 'disconnected'
      socket = null
      remoteCursorAnchors.value = {}
      lastSentCursorUpdate = ''
      scheduleReconnect()
    }
  }

  function toggleManualOffline() {
    if (isManuallyOffline.value) {
      isManuallyOffline.value = false
      reconnectAttempt = 0
      statusMessage.value = 'Reconnecting'
      void connectSocket()
    } else {
      isManuallyOffline.value = true
      clearReconnectTimer()
      disconnectSocket()
      socketState.value = 'disconnected'
      remoteCursorAnchors.value = {}
      lastSentCursorUpdate = ''
      statusMessage.value = 'Manually offline — edits will queue locally'
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
    if (cursorUpdateTimer) {
      window.clearTimeout(cursorUpdateTimer)
      cursorUpdateTimer = null
    }
    disconnectSocket()
    engine?.destroy()
    engine = null
  })

  return {
    canRedo,
    canUndo,
    commitTitle,
    cookieDisplayCount,
    cookieServerCount,
    connectionLabel,
    editorText,
    isManuallyOffline,
    remoteCursors,
    saveDocument,
    saveLabel,
    selectionRestore,
    serverUrl,
    socketState,
    statusMessage,
    title: titleRegister.value,
    clickCookie,
    connectSocket,
    redoEdit,
    toggleManualOffline,
    undoEdit,
    updateSelection,
    updateText,
  }
}
