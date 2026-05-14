# syntax=docker/dockerfile:1

FROM gcc:14
WORKDIR /usr/src/myapp
COPY main.cpp ./
COPY --from=shared . ./shared
RUN g++ -std=c++20 -Ishared main.cpp shared/crdt/GCounter.cpp -o main
CMD ["./main"]
