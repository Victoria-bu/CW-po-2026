FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    g++ cmake curl ca-certificates nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fL -o /usr/local/include/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.15.3/httplib.h

WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

CMD ["./build/search_server"]