<script setup lang="ts">
import DocumentTitle from '../components/DocumentTitle.vue'
import CookieClickerPanel from '../components/CookieClickerPanel.vue'
import EditorToolbar from '../components/EditorToolbar.vue'
import PlainTextEditor from '../components/PlainTextEditor.vue'
import ServerSettings from '../components/ServerSettings.vue'
import { useCollaborativeTextEditor } from '../composables/useCollaborativeTextEditor'

const editor = useCollaborativeTextEditor()
</script>

<template>
  <main class="app-shell">
    <div class="app-layout">
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

        <ServerSettings
          v-model:server-url="editor.serverUrl.value"
          :is-manually-offline="editor.isManuallyOffline.value"
          @reconnect="editor.connectSocket"
          @toggle-offline="editor.toggleManualOffline"
        />

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

      <CookieClickerPanel
        :disabled="false"
        :total="editor.cookieDisplayCount.value"
        @click="editor.clickCookie"
      />
    </div>
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

.app-layout {
  display: grid;
  grid-template-columns: minmax(0, 920px) minmax(240px, 300px);
  align-items: stretch;
  justify-content: center;
  gap: 14px;
}

.editor-card {
  min-height: calc(100vh - 72px);
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

@media (max-width: 1120px) {
  .app-layout {
    grid-template-columns: minmax(0, 920px);
  }
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
