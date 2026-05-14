import { computed, ref } from 'vue'

// Mirrors the C++ crdt::LWWRegister<std::string> at shared/crdt/lww_register.cpp.
// The frontend keeps its own replica for immediate UI updates; the server is
// the authority that broadcasts the canonical merge.

export type LWWMetadata = {
  timestamp: number
  peerId: number
}

export type LWWOperation<T> = {
  value: T
  metadata: LWWMetadata
}

// Total order: timestamp first, peer_id as deterministic tiebreaker.
// Identical to is_newer() in shared/crdt/lww_register.cpp.
export function isNewer(candidate: LWWMetadata, current: LWWMetadata): boolean {
  if (candidate.timestamp !== current.timestamp) {
    return candidate.timestamp > current.timestamp
  }
  return candidate.peerId > current.peerId
}

export function useLWWRegister<T>(peerId: number, initialValue: T) {
  const value = ref(initialValue) as { value: T }
  const metadata = ref<LWWMetadata>({ timestamp: 0, peerId })
  const clock = ref(0)

  // Local write: tick the Lamport clock, return the broadcastable operation.
  function set(newValue: T): LWWOperation<T> {
    clock.value += 1
    metadata.value = { timestamp: clock.value, peerId }
    value.value = newValue
    return { value: newValue, metadata: { ...metadata.value } }
  }

  // Remote write: observe the incoming timestamp into the clock, then keep
  // whichever write wins under isNewer().
  function apply(op: LWWOperation<T>): boolean {
    clock.value = Math.max(clock.value, op.metadata.timestamp) + 1
    if (isNewer(op.metadata, metadata.value)) {
      value.value = op.value
      metadata.value = { ...op.metadata }
      return true
    }
    return false
  }

  return {
    value: computed(() => value.value),
    metadata: computed(() => metadata.value),
    set,
    apply,
  }
}
