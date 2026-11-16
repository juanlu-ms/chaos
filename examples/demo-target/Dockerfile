FROM python:3.13-slim
# CHAOS injects faults from the host's view of the container's namespaces, so
# the app itself needs no root.
RUN useradd --create-home --uid 10001 app
WORKDIR /app
COPY server.py server_downstream.py ./
ENV SERVER_SCRIPT=server.py
USER app
CMD python -u "${SERVER_SCRIPT}"
