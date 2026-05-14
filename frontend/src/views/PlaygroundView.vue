<script setup lang="ts">
import { createGCounter } from '@/lib/crdt'

async function hello() {
    const counter = await createGCounter()
    counter.increment('node-a', 2)
    counter.increment('node-b', 3)
    console.log(counter.value())
    counter.delete()
}

</script>
<template>
    <button @click="hello">
        Do shit
    </button>
</template>
<style scoped>
</style>