<script setup lang="ts">
import { nextTick, ref, watch } from 'vue'

type TextSelection = {
  start: number
  end: number
}

type SelectionRestore = TextSelection & {
  revision: number
}

const props = defineProps<{
  modelValue: string
  selectionRestore: SelectionRestore | null
}>()

const emit = defineEmits<{
  'update:modelValue': [value: string]
  'selection-change': [selection: TextSelection]
}>()

const textarea = ref<HTMLTextAreaElement | null>(null)

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
</script>

<template>
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
    @select="emitSelection()"
  ></textarea>
</template>

<style scoped>
.text-editor {
  display: block;
  width: calc(100% - 36px);
  min-height: 560px;
  margin: 0 18px 18px;
  padding: 18px 16px;
  resize: vertical;
  color: #111827;
  background: #ffffff;
  border: 1px dashed #2088ff;
  border-radius: 4px;
  outline: none;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 1rem;
  line-height: 1.55;
}

.text-editor:focus {
  border-style: solid;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
}

@media (max-width: 760px) {
  .text-editor {
    min-height: 460px;
  }
}
</style>
