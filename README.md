# Nettverksprosjekt - Conflict-free replicated data type

## Getting Started

Kjor hele prosjektet fra prosjektroten med:

```bash
docker compose up -d --build
```

For bare backend:

```bash
docker compose build backend
docker compose run --rm backend
```

Se backend logg med:
```bash 
docker compose logs backend
```

## Arkitektur

### Overordnet

Vi bruker `compose.yaml` for a kjore prosjektet.

### Frontend

Vi har frontend under `frontend/` i roten av prosjektet.
Vi kjorer frontenden med en `Dockerfile`.
Frontenden bruker vue.

### Backend

Vi har backend under `backend/` i roten av prosjektet.
Vi kjorer backenden med en `Dockerfile`.
Backenden bruker C++ med `g++` og delt kode fra `shared/`.
