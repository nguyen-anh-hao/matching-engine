FROM debian:bookworm-slim AS build
WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ cmake make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENGINE_WARNINGS_AS_ERRORS=ON \
    && cmake --build build -j"$(nproc)"

FROM debian:bookworm-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system exchange \
    && useradd --system --gid exchange --no-create-home exchange

WORKDIR /app
COPY --from=build /app/build/matching-engine /usr/local/bin/matching-engine

USER exchange
EXPOSE 9001
HEALTHCHECK --interval=10s --timeout=2s --start-period=5s --retries=3 \
    CMD curl -fsS http://127.0.0.1:9001/health || exit 1

ENTRYPOINT ["matching-engine"]
