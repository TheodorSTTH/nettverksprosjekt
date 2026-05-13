FROM gcc:14
COPY . /usr/src/myapp
WORKDIR /usr/src/myapp
RUN g++ -std=c++20 main.cpp -o main
EXPOSE 3030
CMD ["./main"]
