<script setup lang="ts">
import EditorToolbar from '../components/EditorToolbar.vue'
import PlainTextEditor from '../components/PlainTextEditor.vue'
import ServerSettings from '../components/ServerSettings.vue'
import { useCollaborativeTextEditor } from '../composables/useCollaborativeTextEditor'

const editor = useCollaborativeTextEditor()
</script>

<template>
  <main class="app-shell">
    <section class="editor-card" aria-label="Plain text editor">
      <ServerSettings
        :connection-label="editor.connectionLabel.value"
        :server-url="editor.serverUrl.value"
        :socket-state="editor.socketState.value"
        :user-name="editor.userName.value"
        @reconnect="editor.connectSocket"
        @update:server-url="editor.serverUrl.value = $event"
        @update:user-name="editor.updateUserName"
      />

      <div class="editor-meta">
        <div>
          <h1>Shared text file</h1>
          <p>{{ editor.statusMessage.value }}</p>
        </div>
        <span class="save-state">{{ editor.saveLabel.value }}</span>
      </div>

      <EditorToolbar
        :can-redo="editor.canRedo.value"
        :can-undo="editor.canUndo.value"
        :font-size="editor.fontSize.value"
        @redo="editor.redoEdit"
        @save="editor.saveDocument"
        @set-font-size="editor.setFontSize"
        @undo="editor.undoEdit"
      />

      <PlainTextEditor
        :chars="editor.baseChars.value"
        :client-id="editor.clientId.value"
        :font-size="editor.fontSize.value"
        :model-value="editor.editorText.value"
        :users="editor.visibleRemoteUsers.value"
        @cursor-change="editor.updateCursor"
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
  align-items: end;
  justify-content: space-between;
  gap: 16px;
  padding: 22px 18px 12px;
}

.editor-meta h1 {
  margin: 0;
  color: #25292f;
  font-size: clamp(1.6rem, 1.25rem + 1vw, 2.2rem);
  line-height: 1.12;
  letter-spacing: 0;
}

.editor-meta p {
  margin: 7px 0 0;
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
