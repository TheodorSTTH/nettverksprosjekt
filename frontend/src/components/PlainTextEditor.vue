<script setup lang="ts">
import { computed } from 'vue'
import type { RemoteCharacter, RemoteUser } from '../composables/useCollaborativeTextEditor'

const props = defineProps<{
  chars: RemoteCharacter[]
  clientId: string
  fontSize: number
  modelValue: string
  users: RemoteUser[]
}>()

const emit = defineEmits<{
  cursorChange: [index: number]
  'update:modelValue': [value: string]
}>()

const remoteCursors = computed(() =>
  props.users
    .filter((user) => user.clientId !== props.clientId)
    .map((user) => {
      const charIndex = props.chars.findIndex((character) => character.id === user.cursor.afterId)
      const index = user.cursor.afterId === '' ? 0 : Math.max(0, charIndex + 1)
      const row = Math.floor(index / 72)
      const column = index % 72

      return {
        ...user,
        top: 18 + row * (props.fontSize * 1.55),
        left: 16 + column * Math.max(7, props.fontSize * 0.52),
      }
    }),
)

function emitCursor(event: Event) {
  const target = event.target as HTMLTextAreaElement
  emit('cursorChange', target.selectionStart ?? 0)
}

function emitInput(event: Event) {
  const target = event.target as HTMLTextAreaElement
  emit('update:modelValue', target.value)
  emit('cursorChange', target.selectionStart ?? 0)
}
</script>

<template>
  <div class="editor-wrap">
    <textarea
      class="text-editor"
      :style="{ fontSize: `${fontSize}px` }"
      :value="modelValue"
      aria-label="Plain text content"
      placeholder="Start writing your shared text file here."
      spellcheck="true"
      @click="emitCursor"
      @input="emitInput"
      @keyup="emitCursor"
      @select="emitCursor"
    ></textarea>

    <div class="cursor-layer" aria-hidden="true">
      <div
        v-for="user in remoteCursors"
        :key="user.clientId"
        class="remote-cursor"
        :style="{ transform: `translate(${user.left}px, ${user.top}px)` }"
      >
        <span class="cursor-line"></span>
        <span class="cursor-label">{{ user.userName }}</span>
      </div>
    </div>
  </div>
</template>

<style scoped>
.editor-wrap {
  position: relative;
  margin: 0 18px 18px;
}

.text-editor {
  display: block;
  width: 100%;
  min-height: 560px;
  padding: 18px 16px;
  resize: vertical;
  color: #111827;
  background: #ffffff;
  border: 1px dashed #2088ff;
  border-radius: 4px;
  outline: none;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  line-height: 1.55;
}

.text-editor:focus {
  border-style: solid;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
}

.cursor-layer {
  position: absolute;
  inset: 0;
  overflow: hidden;
  pointer-events: none;
}

.remote-cursor {
  position: absolute;
  top: 0;
  left: 0;
}

.cursor-line {
  display: block;
  width: 2px;
  height: 1.5em;
  background: #dc2626;
}

.cursor-label {
  position: absolute;
  top: -21px;
  left: 0;
  max-width: 140px;
  padding: 2px 6px;
  overflow: hidden;
  color: #ffffff;
  font-size: 0.75rem;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
  background: #dc2626;
  border-radius: 4px;
}

@media (max-width: 760px) {
  .text-editor {
    min-height: 460px;
  }
}
</style>
