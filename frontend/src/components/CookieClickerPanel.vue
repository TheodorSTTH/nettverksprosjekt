<script setup lang="ts">
import backgroundImage from '../../assets/background.png'
import cookieImage from '../../assets/cookie.png'

defineProps<{
  disabled: boolean
  total: number
}>()

const emit = defineEmits<{
  click: []
}>()
</script>

<template>
  <aside
    class="cookie-panel"
    :style="{ backgroundImage: `url(${backgroundImage})` }"
    aria-label="Cookie counter"
  >
    <div class="cookie-total">
      <span class="total-value">{{ total.toLocaleString() }}</span>
      <span class="total-label">server cookies</span>
    </div>

    <button
      class="cookie-button"
      type="button"
      :disabled="disabled"
      aria-label="Add cookie"
      @click="emit('click')"
    >
      <img :src="cookieImage" alt="" draggable="false" />
    </button>
  </aside>
</template>

<style scoped>
.cookie-panel {
  position: sticky;
  top: 36px;
  display: grid;
  grid-template-rows: auto 1fr;
  justify-items: center;
  gap: 20px;
  height: calc(100vh - 72px);
  min-height: calc(100vh - 72px);
  padding: 26px 22px;
  overflow: hidden;
  background-color: #49a7d6;
  background-position: center;
  background-size: cover;
  border: 1px solid #d7e2ea;
  border-radius: 8px;
  box-shadow: 0 18px 45px rgba(31, 41, 55, 0.08);
}

.cookie-total {
  display: grid;
  justify-items: center;
  gap: 2px;
  width: 100%;
  padding: 13px 12px;
  color: #193044;
  background: rgba(255, 255, 255, 0.84);
  border: 1px solid rgba(255, 255, 255, 0.68);
  border-radius: 8px;
  box-shadow: 0 8px 20px rgba(25, 48, 68, 0.14);
}

.total-value {
  font-size: clamp(2rem, 1.4rem + 2vw, 3.25rem);
  font-weight: 800;
  line-height: 1;
}

.total-label {
  color: #475569;
  font-size: 0.9rem;
  font-weight: 700;
}

.cookie-button {
  align-self: center;
  display: grid;
  place-items: center;
  width: min(82%, 250px);
  aspect-ratio: 1;
  padding: 0;
  cursor: pointer;
  background: transparent;
  border: 0;
  border-radius: 999px;
  filter: drop-shadow(0 18px 22px rgba(79, 43, 18, 0.26));
  transition:
    filter 120ms ease,
    transform 120ms ease;
}

.cookie-button:hover:not(:disabled) {
  filter: drop-shadow(0 21px 24px rgba(79, 43, 18, 0.3));
  transform: scale(1.035);
}

.cookie-button:active:not(:disabled) {
  transform: scale(0.96);
}

.cookie-button:focus-visible {
  outline: 3px solid rgba(255, 255, 255, 0.92);
  outline-offset: 5px;
}

.cookie-button:disabled {
  cursor: not-allowed;
  opacity: 0.62;
}

.cookie-button img {
  display: block;
  width: 100%;
  height: 100%;
  user-select: none;
}

@media (max-width: 1120px) {
  .cookie-panel {
    position: static;
    height: auto;
    min-height: 420px;
  }
}
</style>
