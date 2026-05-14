import EditorView from '@/views/EditorView.vue'
import PlaygroundView from '@/views/PlaygroundView.vue'
import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      component: EditorView,
    },
    {
      path: '/playground',
      component: PlaygroundView,
    },
  ],
})

export default router
