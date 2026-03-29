FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source
COPY . .

# Build
RUN make clean && make

# Create data directories
RUN mkdir -p data backups

# Copy config
RUN cp config.env data/ 2>/dev/null || true

EXPOSE 8080 8081

ENTRYPOINT ["./docker-entrypoint.sh"]
