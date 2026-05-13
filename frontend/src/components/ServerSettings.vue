<script setup lang="ts">
import type { SocketState } from '../composables/useCollaborativeTextEditor'

defineProps<{
  connectionLabel: string
  serverUrl: string
  socketState: SocketState
  userName: string
}>()

defineEmits<{
  reconnect: []
  'update:serverUrl': [value: string]
  'update:userName': [value: string]
}>()
</script>

<template>
  <div class="server-controls" aria-label="Server settings">
    <label class="field user-field">
      <span>Name</span>
      <input
        :value="userName"
        type="text"
        spellcheck="false"
        @input="$emit('update:userName', ($event.target as HTMLInputElement).value)"
      />
    </label>

    <label class="field">
      <span>Server</span>
      <input
        :value="serverUrl"
        type="text"
        spellcheck="false"
        @change="$emit('reconnect')"
        @input="$emit('update:serverUrl', ($event.target as HTMLInputElement).value)"
      />
    </label>

    <button class="reconnect-button" type="button" @click="$emit('reconnect')">
      <svg viewBox="0 0 24 24" aria-hidden="true">
        <path d="M21 12a9 9 0 0 0-15.4-6.4L3 8" />
        <path d="M3 3v5h5" />
        <path d="M3 12a9 9 0 0 0 15.4 6.4L21 16" />
        <path d="M16 16h5v5" />
      </svg>
      Reconnect
    </button>

    <div class="connection-status" :data-state="socketState">
      <span class="status-dot" aria-hidden="true"></span>
      <span>{{ connectionLabel }}</span>
    </div>
  </div>
</template>

<style scoped>
.server-controls {
  display: grid;
  grid-template-columns: minmax(150px, 220px) minmax(240px, 1fr) auto auto;
  align-items: end;
  gap: 12px;
  padding: 16px 18px 4px;
  border-bottom: 1px solid #eef1f5;
}

.field {
  display: grid;
  gap: 6px;
  min-width: 0;
  color: #596373;
  font-size: 0.82rem;
  font-weight: 650;
}

.field input {
  width: 100%;
  min-width: 0;
  height: 38px;
  padding: 0 11px;
  color: #1f252d;
  border: 1px solid #d9dfe7;
  border-radius: 6px;
  outline: none;
  background: #ffffff;
}

.field input:focus {
  border-color: #2088ff;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.16);
}

.reconnect-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  height: 38px;
  padding: 0 11px;
  color: #46505c;
  font-weight: 650;
  border: 1px solid transparent;
  border-radius: 6px;
  background: transparent;
  cursor: pointer;
}

.reconnect-button:hover {
  background: #f2f4f7;
  border-color: #d8dee7;
}

.reconnect-button svg {
  width: 19px;
  height: 19px;
  fill: none;
  stroke: currentColor;
  stroke-linecap: round;
  stroke-linejoin: round;
  stroke-width: 2;
}

.connection-status {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-height: 38px;
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

@media (max-width: 820px) {
  .server-controls {
    grid-template-columns: 1fr;
  }

  .reconnect-button,
  .connection-status {
    width: 100%;
  }
}
</style>
