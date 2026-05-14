<script setup lang="ts">
import DocumentTitle from '../components/DocumentTitle.vue'
import EditorToolbar from '../components/EditorToolbar.vue'
import PlainTextEditor from '../components/PlainTextEditor.vue'
import ServerSettings from '../components/ServerSettings.vue'
import { useCollaborativeTextEditor } from '../composables/useCollaborativeTextEditor'

const editor = useCollaborativeTextEditor()
</script>

<template>
  <main class="app-shell">
    <section class="editor-card" aria-label="Plain text editor">
      <EditorToolbar
        :can-redo="editor.canRedo.value"
        :can-undo="editor.canUndo.value"
        :connection-label="editor.connectionLabel.value"
        :socket-state="editor.socketState.value"
        @reconnect="editor.connectSocket"
        @redo="editor.redoEdit"
        @save="editor.saveDocument"
        @undo="editor.undoEdit"
      />

      <ServerSettings v-model:server-url="editor.serverUrl.value" @reconnect="editor.connectSocket" />

      <DocumentTitle :model-value="editor.title.value" @commit="editor.commitTitle" />

      <div class="editor-meta">
        <p>{{ editor.statusMessage.value }}</p>
        <span class="save-state">{{ editor.saveLabel.value }}</span>
      </div>

      <PlainTextEditor
        :model-value="editor.editorText.value"
        :remote-cursors="editor.remoteCursors.value"
        :selection-restore="editor.selectionRestore.value"
        @selection-change="editor.updateSelection"
        @update:model-value="editor.updateText"
      />
    </section>
  </main>
</template>

<style scoped>
.app-shell {
  min-height: 100vh;
  padding: 36px 20px;
  background:
    linear-gradient(180deg, rgba(255, 255, 255, 0.45), rgba(255, 255, 255, 0) 280px),
    #f3f4f6;
}

.editor-card {
  width: min(100%, 920px);
  min-height: calc(100vh - 72px);
  margin: 0 auto;
  overflow: hidden;
  background: #ffffff;
  border: 1px solid #e0e4ea;
  border-radius: 8px;
  box-shadow: 0 18px 45px rgba(31, 41, 55, 0.08);
}

.editor-meta {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 4px 18px 12px;
}

.editor-meta p {
  margin: 0;
  color: #687383;
  font-size: 0.96rem;
}

.save-state {
  flex: 0 0 auto;
  color: #596373;
  font-size: 0.9rem;
  white-space: nowrap;
}

@media (max-width: 760px) {
  .app-shell {
    padding: 14px;
  }

  .editor-card {
    min-height: calc(100vh - 28px);
  }

  .editor-meta {
    align-items: flex-start;
    flex-direction: column;
  }

  .save-state {
    white-space: normal;
  }
}
</style>
