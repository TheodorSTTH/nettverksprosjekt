# Backend

Backend bruker `backend/Dockerfile` og delt kode fra `../shared`.
Alle `.cpp`-filer i `shared/crdt/` blir bygget automatisk.

Bygg fra `backend/` med ekstra build context for `shared/`:

```bash
docker build --build-context shared=../shared -t backend-crdt .
```

Kjor containeren:

```bash
docker run --rm backend-crdt
```

Bygg & kjør:
```bash
docker build --build-context shared=../shared -t backend-crdt . && docker run --rm backend-crdt
```
