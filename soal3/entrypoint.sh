#!/bin/bash
set -e

MODE="$1"

setup_users_and_permissions() {
    mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs

    groupadd -f readonly

    id member >/dev/null 2>&1 || useradd -m -s /bin/bash -G readonly member
    id contributor >/dev/null 2>&1 || useradd -m -s /bin/bash -G staff contributor
    id librarian >/dev/null 2>&1 || useradd -m -s /bin/bash -G staff librarian

    echo "member:member123" | chpasswd
    echo "contributor:contrib456" | chpasswd
    echo "librarian:lib789" | chpasswd

    printf "member123\nmember123\n" | smbpasswd -a -s member >/dev/null 2>&1 || true
    printf "contrib456\ncontrib456\n" | smbpasswd -a -s contributor >/dev/null 2>&1 || true
    printf "lib789\nlib789\n" | smbpasswd -a -s librarian >/dev/null 2>&1 || true

    smbpasswd -e member >/dev/null 2>&1 || true
    smbpasswd -e contributor >/dev/null 2>&1 || true
    smbpasswd -e librarian >/dev/null 2>&1 || true

    chown -R root:staff /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs

    chmod 2775 /libraryit/ebooks
    chmod 2775 /libraryit/papers
    chmod 2775 /libraryit/sourcecode
    chmod 0555 /libraryit/docs

    touch /logs/libraryit.log
    chmod 666 /logs/libraryit.log
}

run_logger() {
    LOG_FILE="/logs/libraryit.log"

    mkdir -p /logs
    touch "$LOG_FILE"
    chmod 666 "$LOG_FILE"

    echo "[LOGGER] libraryit-logger started" >> "$LOG_FILE"

    inotifywait -m -r /libraryit \
        -e create -e modify -e delete -e moved_to -e moved_from \
        --format '%T|%e|%w%f' \
        --timefmt '%Y-%m-%d %H:%M:%S' | while IFS='|' read -r timestamp event filepath
    do
        share="unknown"

        case "$filepath" in
            /libraryit/ebooks/*)
                share="ebooks"
                ;;
            /libraryit/papers/*)
                share="papers"
                ;;
            /libraryit/sourcecode/*)
                share="SourceCode"
                ;;
            /libraryit/docs/*)
                share="docs"
                ;;
        esac

        action="ACCESS"

        if echo "$event" | grep -q "CREATE\|MOVED_TO"; then
            action="CREATE"
        elif echo "$event" | grep -q "MODIFY"; then
            action="WRITE"
        elif echo "$event" | grep -q "DELETE\|MOVED_FROM"; then
            action="DELETE"
        fi

        filename=$(basename "$filepath")

        echo "[$timestamp] [INFO] [filesystem] [$action] [$share/$filename]" | tee -a "$LOG_FILE"
    done
}

if [ "$MODE" = "logger" ]; then
    run_logger
fi

setup_users_and_permissions

exec smbd -F --no-process-group