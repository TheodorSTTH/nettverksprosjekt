<script setup lang="ts">
defineProps<{
  serverUrl: string
}>()

defineEmits<{
  reconnect: []
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
  </div>
</template>

<style scoped>
.server-controls {
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
</style>
