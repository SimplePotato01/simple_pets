FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    net-tools \
    procps \
    watch \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY src/ ./src/
COPY scripts/ ./scripts/
COPY Makefile .

RUN make

RUN cp bin/tcp_killer /usr/local/bin/ && \
    cp scripts/tcp-killer.sh /usr/local/bin/tcp-killer && \
    chmod +x /usr/local/bin/tcp-killer

ENTRYPOINT ["/usr/local/bin/tcp-killer"]
CMD ["--help"]
