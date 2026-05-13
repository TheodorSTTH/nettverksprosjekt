<script setup lang="ts">
import EditorView from './views/EditorView.vue'
</script>

<template>
  <EditorView />
</template>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  color: #25292f;
  background: #f3f4f6;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}

button,
input,
textarea {
  font: inherit;
}
</style>
