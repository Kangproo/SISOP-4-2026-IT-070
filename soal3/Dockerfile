FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    samba \
    smbclient \
    inotify-tools \
    coreutils \
    procps \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh && \
    mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs && \
    touch /logs/libraryit.log

EXPOSE 445 139

ENTRYPOINT ["/entrypoint.sh"]