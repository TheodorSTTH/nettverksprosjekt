<script setup lang="ts">
import type { SocketState } from '../composables/useCollaborativeTextEditor'

defineProps<{
  canRedo: boolean
  canUndo: boolean
  connectionLabel: string
  socketState: SocketState
}>()

defineEmits<{
  reconnect: []
  redo: []
  save: []
  undo: []
}>()
</script>

<template>
  <header class="top-bar">
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

      <button class="toolbar-button" type="button" @click="$emit('save')">
        <svg viewBox="0 0 24 24" aria-hidden="true">
          <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2Z" />
          <path d="M17 21v-8H7v8" />
          <path d="M7 3v5h8" />
        </svg>
        Save
      </button>

      <button class="toolbar-button secondary" type="button" @click="$emit('reconnect')">
        <svg viewBox="0 0 24 24" aria-hidden="true">
          <path d="M21 12a9 9 0 0 0-15.4-6.4L3 8" />
          <path d="M3 3v5h5" />
          <path d="M3 12a9 9 0 0 0 15.4 6.4L21 16" />
          <path d="M16 16h5v5" />
        </svg>
        Reconnect
      </button>
    </div>

    <div class="connection-status" :data-state="socketState">
      <span class="status-dot" aria-hidden="true"></span>
      <span>{{ connectionLabel }}</span>
    </div>
  </header>
</template>

<style scoped>
.top-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  min-height: 58px;
  padding: 10px 18px;
  border-bottom: 1px solid #e7ebf0;
}

.toolbar {
  display: flex;
  flex: 1;
  align-items: center;
  gap: 6px;
  min-width: 0;
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

.toolbar-button.secondary {
  color: #46505c;
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

.connection-status {
  display: inline-flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 8px;
  min-height: 30px;
  padding: 0 10px;
  color: #596373;
  font-size: 0.9rem;
  white-space: nowrap;
  border: 1px solid #dde3ea;
  border-radius: 999px;
  background: #fafbfc;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 999px;
  background: #a8b0ba;
}

.connection-status[data-state="connected"] .status-dot {
  background: #15803d;
}

.connection-status[data-state="connecting"] .status-dot {
  background: #c27803;
}

.connection-status[data-state="disconnected"] .status-dot {
  background: #b91c1c;
}

@media (max-width: 760px) {
  .top-bar {
    align-items: flex-start;
    flex-direction: column;
  }

  .toolbar {
    flex-wrap: wrap;
  }
}

@media (max-width: 460px) {
  .toolbar-button {
    flex: 1 1 auto;
  }
}
</style>
