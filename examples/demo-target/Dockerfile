FROM python:slim
WORKDIR /app
COPY server.py server_downstream.py ./
ENV SERVER_SCRIPT=server.py
CMD python -u "${SERVER_SCRIPT}"
