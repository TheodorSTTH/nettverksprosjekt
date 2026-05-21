<script setup lang="ts">
defineProps<{
  serverUrl: string
  isManuallyOffline: boolean
}>()

defineEmits<{
  reconnect: []
  'toggle-offline': []
  'update:serverUrl': [value: string]
}>()
</script>

<template>
  <div class="server-controls" aria-label="Server settings">
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

    <button
      type="button"
      class="offline-toggle"
      :class="{ 'is-offline': isManuallyOffline }"
      :title="isManuallyOffline ? 'Reconnect to the server' : 'Disconnect to simulate going offline'"
      @click="$emit('toggle-offline')"
    >
      <span class="dot" aria-hidden="true"></span>
      {{ isManuallyOffline ? 'Go online' : 'Go offline' }}
    </button>
  </div>
</template>

<style scoped>
.server-controls {
  display: grid;
  grid-template-columns: 1fr auto;
  align-items: end;
  gap: 12px;
  padding: 16px 18px 4px;
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

.offline-toggle {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  height: 38px;
  padding: 0 14px;
  border: 1px solid #d9dfe7;
  border-radius: 6px;
  background: #ffffff;
  color: #1f252d;
  font-size: 0.85rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s ease, border-color 0.15s ease, color 0.15s ease;
}

.offline-toggle:hover {
  border-color: #b6c0cc;
  background: #f6f8fb;
}

.offline-toggle:focus-visible {
  outline: none;
  border-color: #2088ff;
  box-shadow: 0 0 0 3px rgba(32, 136, 255, 0.18);
}

.offline-toggle .dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: #16a34a;
  box-shadow: 0 0 0 2px rgba(22, 163, 74, 0.18);
}

.offline-toggle.is-offline {
  border-color: #f0b4b4;
  background: #fff5f5;
  color: #a3262a;
}

.offline-toggle.is-offline .dot {
  background: #dc2626;
  box-shadow: 0 0 0 2px rgba(220, 38, 38, 0.18);
}
</style>
