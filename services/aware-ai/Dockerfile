# Aware AI sidecar. Targets: "runtime" (default, served by compose) and "dev" (tests, lint, type checks).
FROM python:3.12.7-slim-bookworm AS base
COPY --from=ghcr.io/astral-sh/uv:0.5.4 /uv /usr/local/bin/uv
# The venv lives outside /app so a bind-mounted source tree in dev does not hide it.
ENV UV_PROJECT_ENVIRONMENT=/opt/venv \
    UV_COMPILE_BYTECODE=1 \
    UV_LINK_MODE=copy \
    PATH="/opt/venv/bin:$PATH" \
    PYTHONUNBUFFERED=1
WORKDIR /app

FROM base AS dev
COPY pyproject.toml uv.lock README.md ./
RUN uv sync --frozen --no-install-project
COPY . .
RUN uv sync --frozen

FROM base AS runtime
COPY pyproject.toml uv.lock README.md ./
RUN uv sync --frozen --no-dev --no-install-project
COPY src ./src
COPY templates ./templates
RUN uv sync --frozen --no-dev
# Same uid/gid as the host user (AzerothCore's DOCKER_USER_ID convention), so the bind-mounted log folder is
# writable here and readable on the host.
ARG USER_ID=1000
ARG GROUP_ID=1000
RUN groupadd --system --gid "${GROUP_ID}" aware-ai \
    && useradd --system --uid "${USER_ID}" --gid "${GROUP_ID}" aware-ai \
    && mkdir -p /data /logs && chown aware-ai:aware-ai /data /logs
USER aware-ai
ENV AWARE_AI_DATA_DIR=/data \
    AWARE_AI_LOG_DIR=/logs \
    AWARE_AI_TEMPLATE_DIR=/app/templates
EXPOSE 8090
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
    CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8090/health', timeout=2)"]
CMD ["uvicorn", "aware_ai.main:create_app", "--factory", "--host", "0.0.0.0", "--port", "8090", "--no-access-log"]
