<script setup lang="ts">
import { nextTick, onMounted, ref, watch } from 'vue'

type TextSelection = {
  start: number
  end: number
}

type SelectionRestore = TextSelection & {
  revision: number
}

type RemoteEditorCursor = {
  peerId: number
  label: string
  color: string
  cursorIndex: number
}

type RemoteCaret = RemoteEditorCursor & {
  left: number
  top: number
}

const props = defineProps<{
  modelValue: string
  selectionRestore: SelectionRestore | null
  remoteCursors: RemoteEditorCursor[]
}>()

const emit = defineEmits<{
  'update:modelValue': [value: string]
  'selection-change': [selection: TextSelection]
}>()

const textarea = ref<HTMLTextAreaElement | null>(null)
const mirror = ref<HTMLDivElement | null>(null)
const remoteCarets = ref<RemoteCaret[]>([])

function emitSelection(target: HTMLTextAreaElement | null = textarea.value) {
  if (!target) {
    return
  }

  emit('selection-change', {
    start: target.selectionStart,
    end: target.selectionEnd,
  })
}

function handleInput(event: Event) {
  const target = event.target as HTMLTextAreaElement
  emit('update:modelValue', target.value)
  emitSelection(target)
}

function measureCursor(cursorIndex: number) {
  const target = textarea.value
  const mirrorElement = mirror.value
  if (!target || !mirrorElement) {
    return { left: 0, top: 0 }
  }

  const marker = document.createElement('span')
  marker.className = 'presence-marker'
  marker.textContent = '\u200b'

  const clampedIndex = Math.max(0, Math.min(cursorIndex, props.modelValue.length))
  mirrorElement.replaceChildren(document.createTextNode(props.modelValue.slice(0, clampedIndex)), marker)

  return {
    left: marker.offsetLeft - target.scrollLeft,
    top: marker.offsetTop - target.scrollTop,
  }
}

async function updateCaretPositions() {
  await nextTick()

  remoteCarets.value = props.remoteCursors.map((cursor) => ({
    ...cursor,
    ...measureCursor(cursor.cursorIndex),
  }))
}

function handleScroll() {
  void updateCaretPositions()
}

watch(
  () => props.selectionRestore?.revision,
  async () => {
    if (!props.selectionRestore) {
      return
    }

    await nextTick()

    const target = textarea.value
    if (!target || document.activeElement !== target) {
      return
    }

    const start = Math.max(0, Math.min(props.selectionRestore.start, target.value.length))
    const end = Math.max(start, Math.min(props.selectionRestore.end, target.value.length))
    target.setSelectionRange(start, end)
    emitSelection(target)
  },
)

watch(
  [() => props.modelValue, () => props.remoteCursors],
  () => {
    void updateCaretPositions()
  },
  { deep: true },
)

onMounted(() => {
  void updateCaretPositions()
})
</script>

<template>
  <div class="editor-frame">
    <textarea
      ref="textarea"
      class="text-editor"
      :value="modelValue"
      aria-label="Plain text content"
      spellcheck="true"
      @focus="emitSelection()"
      @input="handleInput"
      @keyup="emitSelection()"
      @mouseup="emitSelection()"
      @scroll="handleScroll"
      @select="emitSelection()"
    ></textarea>

    <div class="presence-layer" aria-hidden="true">
      <div
        v-for="caret in remoteCarets"
        :key="caret.peerId"
        class="presence-caret"
        :style="{ left: `${caret.left}px`, top: `${caret.top}px`, '--presence-color': caret.color }"
      >
        <span class="presence-label">{{ caret.label }}</span>
      </div>
    </div>

    <div ref="mirror" class="textarea-mirror" aria-hidden="true"></div>
  </div>
</template>

<style scoped>
.editor-frame {
  position: relative;
  width: calc(100% - 36px);
  min-height: 560px;
  margin: 0 18px 18px;
}

.text-editor,
.textarea-mirror {
  box-sizing: border-box;
  width: 100%;
  min-height: 560px;
  padding: 18px 16px;
  border: 1px dashed #2088ff;
  border-radius: 4px;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 1rem;
  line-height: 1.55;
  white-space: pre-wrap;
  overflow-wrap: break-word;
}

.text-editor {
  position: relative;
  z-index: 1;
  display: block;
  resize: vertical;
  color: #111827;
  background: #ffffff;
  outline: none;
}

.text-editor:focus {
  border-style: solid;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
}

.presence-layer {
  position: absolute;
  z-index: 2;
  inset: 0;
  overflow: hidden;
  pointer-events: none;
}

.presence-caret {
  position: absolute;
  width: 2px;
  height: 1.55rem;
  background: var(--presence-color);
}

.presence-label {
  position: absolute;
  bottom: 100%;
  left: 0;
  max-width: 96px;
  padding: 2px 5px;
  overflow: hidden;
  color: #ffffff;
  background: var(--presence-color);
  border-radius: 3px;
  font-size: 0.72rem;
  line-height: 1.2;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.textarea-mirror {
  position: absolute;
  inset: 0 auto auto 0;
  visibility: hidden;
  pointer-events: none;
}

@media (max-width: 760px) {
  .editor-frame,
  .text-editor,
  .textarea-mirror {
    min-height: 460px;
  }
}
</style>
