FROM python:slim
WORKDIR /app
COPY server.py .
# -u ensures unbuffered stdout so logs bypass Docker's buffer and appear instantly in real-time
CMD ["python", "-u", "server.py"]
