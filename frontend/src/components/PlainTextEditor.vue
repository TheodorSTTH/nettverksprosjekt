<script setup lang="ts">
defineProps<{
  modelValue: string
}>()

defineEmits<{
  'update:modelValue': [value: string]
}>()
</script>

<template>
  <textarea
    class="text-editor"
    :value="modelValue"
    aria-label="Plain text content"
    spellcheck="true"
    @input="$emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
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
