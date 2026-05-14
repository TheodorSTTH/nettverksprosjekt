<script setup lang="ts">
import { ref, watch } from 'vue'

const props = defineProps<{
  modelValue: string
}>()

const emit = defineEmits<{
  commit: [value: string]
}>()

// Local draft state. While the user is typing, we do NOT propagate keystrokes
// to the CRDT — that would cause jarring overwrites between concurrent editors.
// We only emit `commit` on blur or Enter, at which point the LWW merge resolves.
const draft = ref(props.modelValue)
const isEditing = ref(false)

// When a remote LWW update arrives and the user is not editing, pull it in.
// If the user IS editing, we leave their draft alone — their commit will
// resolve via LWW against the latest timestamp.
watch(
  () => props.modelValue,
  (next) => {
    if (!isEditing.value) {
      draft.value = next
    }
  },
)

function onFocus() {
  isEditing.value = true
}

function onBlur() {
  isEditing.value = false
  const trimmed = draft.value.trim() || 'Untitled'
  draft.value = trimmed
  if (trimmed !== props.modelValue) {
    emit('commit', trimmed)
  }
}

function onEnter(event: KeyboardEvent) {
  event.preventDefault()
  ;(event.target as HTMLInputElement).blur()
}

function onEscape(event: KeyboardEvent) {
  event.preventDefault()
  draft.value = props.modelValue
  ;(event.target as HTMLInputElement).blur()
}
</script>

<template>
  <div class="document-title">
    <input
      v-model="draft"
      class="document-title__input"
      type="text"
      maxlength="120"
      aria-label="Document title"
      placeholder="Untitled"
      @focus="onFocus"
      @blur="onBlur"
      @keydown.enter="onEnter"
      @keydown.escape="onEscape"
    />
  </div>
</template>

<style scoped>
.document-title {
  display: flex;
  justify-content: center;
  padding: 24px 18px 8px;
}

.document-title__input {
  width: 100%;
  max-width: 720px;
  padding: 6px 10px;
  border: 1px solid transparent;
  border-radius: 6px;
  background: transparent;
  color: #1f2937;
  font-size: clamp(1.6rem, 1.25rem + 1vw, 2.2rem);
  font-weight: 600;
  line-height: 1.15;
  text-align: center;
  outline: none;
  transition:
    border-color 120ms ease,
    background-color 120ms ease;
}

.document-title__input::placeholder {
  color: #9aa3b2;
  font-weight: 500;
}

.document-title__input:hover {
  border-color: #e0e4ea;
}

.document-title__input:focus {
  border-color: #2088ff;
  background: #ffffff;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
  text-align: left;
}
</style>
