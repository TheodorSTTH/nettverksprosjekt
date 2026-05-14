# Nettverksprosjekt - Conflict-free replicated data type

## Getting Started

Kjor hele prosjektet fra prosjektroten med:

```bash
docker compose up --build
```

Apne appen i nettleseren:

- Frontend: `http://localhost:8080`
- Backend WebSocket: `ws://localhost:3030/ws`

Kjor i bakgrunnen:

```bash
docker compose up -d --build
```

Stopp prosjektet:

```bash
docker compose down
```

Se logger med:

```bash 
docker compose logs -f
```

## Arkitektur

### Overordnet

Vi bruker `compose.yaml` for a kjore prosjektet.

### Frontend

Vi har frontend under `frontend/` i roten av prosjektet.
Vi kjorer frontenden med `frontend/Dockerfile`.
Frontenden bruker Vue og bygger CRDT WebAssembly fra delt C++ kode under Docker-builden.

### Backend

Vi har backend under `backend/` i roten av prosjektet.
Vi kjorer backenden med `backend/Dockerfile`.
Backenden bygges med CMake og bruker delt CRDT/protokoll/server-kode fra `shared/`.
