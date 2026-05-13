# Nettverksprosjekt - Collaborative CRDT Text Editor

## Introduksjon

Dette prosjektet er en enkel samarbeids-editor for tekstfiler. Flere brukere kan koble seg til samme server via WebSocket, skrive i samme dokument, se andre aktive brukere, og endre felles fontstørrelse.

Målet er å demonstrere Conflict-free Replicated Data Types (CRDT) i en klient-server-arkitektur. CRDT-logikken er implementert selv i C++ uten eksterne CRDT-biblioteker.

## Arkitektur

- `frontend/`: Vue 3 + Vite-applikasjon.
- `backend/`: C++20 backend bygget med CMake.
- `compose.yaml`: kjører frontend og backend med Docker Compose.

Frontend sender tekstendringer som operasjoner til backend. Backend eier dokumenttilstanden, merger operasjonene med CRDT-strukturen, og broadcaster nye snapshots til alle tilkoblede klienter.

## Implementert Funksjonalitet

- WebSocket-server på `0.0.0.0:3030/ws`.
- RGA-inspirert tekst-CRDT:
  - Hvert tegn får en stabil ID.
  - Inserts refererer til tegnet de kommer etter.
  - Deletes markerer tegn som tombstones.
  - Rendering skjer deterministisk ved å traversere CRDT-strukturen.
- LWW-register for fontstørrelse:
  - Serveren tildeler monotone timestamps.
  - Sist aksepterte fontstørrelse vinner.
- Presence:
  - Brukere sender `clientId`, navn og cursor-posisjon.
  - Andre klienter vises med navn og omtrentlig cursor-markør.
- Autosave:
  - Backend lagrer dokumenttilstand til `backend/data/document.json`.
  - Dokumentet lastes inn igjen ved restart.
- Frontend:
  - Serverfelt for LAN-testing.
  - Navnefelt.
  - Undo/redo lokalt.
  - Font-size-kontroll.
  - Save-knapp og 5 sekunders sync.

## Eksterne Avhengigheter

- Boost.Beast / Boost.Asio: WebSocket og TCP-server i C++.
- nlohmann/json: JSON parsing og serialisering i C++.
- Vue: frontend UI.
- Vite: frontend build/dev-server.
- Docker Compose: kjøre hele løsningen lokalt eller på LAN.

Ingen eksterne CRDT-biblioteker brukes.

## Installasjon Og Kjøring

Kjør hele løsningen:

```sh
docker compose up --build
```

Frontend blir tilgjengelig på:

```text
http://localhost:8080
```

Backend WebSocket kjører på:

```text
ws://localhost:3030/ws
```

## LAN-testing Med Flere Enheter

1. Start prosjektet på maskinen som skal være server:

   ```sh
   docker compose up --build
   ```

2. Finn IP-adressen til servermaskinen på lokalnettet.

3. Åpne frontend fra samme maskin eller andre enheter:

   ```text
   http://<server-ip>:8080
   ```

4. Sett serverfeltet i frontend til:

   ```text
   ws://<server-ip>:3030/ws
   ```

5. Skriv inn ulike navn i hver klient og test samtidig redigering.

## Lokal Backend-test

Hvis CMake, Boost og nlohmann/json er installert lokalt:

```sh
cmake -S backend -B backend/build
cmake --build backend/build
ctest --test-dir backend/build
```

## Frontend-test

```sh
cd frontend
npm install
npm run type-check
npm run build
```

## API

Frontend sender edit batches:

```json
{
  "type": "edit:batch",
  "clientId": "client-id",
  "userName": "Alice",
  "baseRevision": 1,
  "ops": [
    { "type": "delete", "ids": ["client:1"] },
    { "type": "insert", "afterId": "client:1", "text": "hello" }
  ],
  "cursor": { "afterId": "client:2" }
}
```

Backend broadcaster snapshots:

```json
{
  "type": "document:snapshot",
  "revision": 2,
  "text": "hello",
  "chars": [{ "id": "client:1", "value": "h" }],
  "fontSize": 16,
  "users": [
    { "clientId": "client-id", "userName": "Alice", "cursor": { "afterId": "client:1" } }
  ]
}
```

## Kjente Begrensninger Og Fremtidig Arbeid

- Frontend diff lager én sammenhengende insert/delete-operasjon per sync. Det fungerer for vanlig tekstinput, men en mer avansert editor bør sende mer presise operasjoner.
- Remote cursors i textarea er omtrentlige fordi native textarea ikke eksponerer presis tegnposisjon for overlay uten ekstra måling.
- Backend kjører ett delt dokument for v1.
- Det finnes ingen autentisering eller tilgangskontroll.
- CRDT-en håndterer plain text byte-for-byte. Full Unicode-grapheme-støtte bør forbedres.
- Neste steg kan være dokumentvalg, bedre cursor-rendering, reconnect/backoff, og mer omfattende CRDT-tester.
