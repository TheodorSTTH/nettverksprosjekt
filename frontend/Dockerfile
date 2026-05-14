FROM emscripten/emsdk:3.1.74 AS wasm

WORKDIR /src

COPY shared ./shared
COPY frontend/scripts ./frontend/scripts
COPY frontend/public ./frontend/public

RUN node frontend/scripts/build-wasm.mjs

FROM node:lts-alpine AS build

WORKDIR /app

COPY frontend/package*.json ./
RUN npm ci

COPY frontend ./
COPY --from=wasm /src/frontend/public/wasm ./public/wasm

RUN npm run type-check \
    && npm run build-only

FROM node:lts-alpine AS runtime

RUN npm install -g http-server

WORKDIR /app
COPY --from=build /app/dist ./dist

EXPOSE 8080
CMD ["http-server", "dist", "-p", "8080", "-a", "0.0.0.0", "-c-1"]
