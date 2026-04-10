FROM ubuntu:latest


RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    g++ \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN g++ -o tinyshell myShell.cpp

CMD ["./tinyshell"]