<script setup lang="ts">
defineProps<{
  canRedo: boolean
  canUndo: boolean
  fontSize: number
}>()

defineEmits<{
  redo: []
  save: []
  'set-font-size': [value: number]
  undo: []
}>()
</script>

<template>
  <div class="toolbar" aria-label="Editor toolbar">
    <button class="icon-button" type="button" :disabled="!canUndo" aria-label="Undo" @click="$emit('undo')">
      <svg viewBox="0 0 24 24" aria-hidden="true">
        <path d="M9 7 4 12l5 5" />
        <path d="M5 12h9a5 5 0 1 1 0 10h-1" />
      </svg>
    </button>

    <button class="icon-button" type="button" :disabled="!canRedo" aria-label="Redo" @click="$emit('redo')">
      <svg viewBox="0 0 24 24" aria-hidden="true">
        <path d="m15 7 5 5-5 5" />
        <path d="M19 12h-9a5 5 0 1 0 0 10h1" />
      </svg>
    </button>

    <span class="toolbar-separator" aria-hidden="true"></span>

    <label class="font-size-field">
      <span>Font size</span>
      <input
        :value="fontSize"
        type="number"
        min="10"
        max="48"
        step="1"
        @change="$emit('set-font-size', Number(($event.target as HTMLInputElement).value))"
      />
    </label>

    <button class="toolbar-button" type="button" @click="$emit('save')">
      <svg viewBox="0 0 24 24" aria-hidden="true">
        <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2Z" />
        <path d="M17 21v-8H7v8" />
        <path d="M7 3v5h8" />
      </svg>
      Save
    </button>
  </div>
</template>

<style scoped>
.toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  padding: 0 18px 14px;
}

.icon-button,
.toolbar-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 34px;
  border: 1px solid transparent;
  border-radius: 6px;
  color: #20242a;
  background: transparent;
  cursor: pointer;
}

.icon-button {
  width: 34px;
  padding: 0;
}

.toolbar-button {
  gap: 7px;
  padding: 0 11px;
  font-weight: 600;
}

.icon-button:hover:not(:disabled),
.toolbar-button:hover {
  background: #f2f4f7;
  border-color: #d8dee7;
}

.icon-button:disabled {
  color: #a8b0ba;
  cursor: not-allowed;
}

.icon-button svg,
.toolbar-button svg {
  width: 19px;
  height: 19px;
  fill: none;
  stroke: currentColor;
  stroke-linecap: round;
  stroke-linejoin: round;
  stroke-width: 2;
}

.toolbar-separator {
  width: 1px;
  height: 24px;
  margin: 0 4px;
  background: #d8dee7;
}

.font-size-field {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  color: #596373;
  font-size: 0.9rem;
  font-weight: 650;
}

.font-size-field input {
  width: 72px;
  height: 34px;
  padding: 0 8px;
  color: #1f252d;
  border: 1px solid #d9dfe7;
  border-radius: 6px;
  outline: none;
}

.font-size-field input:focus {
  border-color: #2088ff;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
}

@media (max-width: 760px) {
  .toolbar {
    flex-wrap: wrap;
  }
}

@media (max-width: 460px) {
  .toolbar-button,
  .font-size-field {
    flex: 1 1 auto;
  }
}
</style>
