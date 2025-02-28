#! /bin/sh

case "$1" in
    start) 
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        if [ $exit_status -ne 0 ]; then
            echo "failed to start aesdsocket"
        else
            echo "started aesdsocket"
        fi
    ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -k -n aesdsocket
        if [ $exit_status -ne 0 ]; then
            echo "failed to stop aesdsocket"
        else
            echo "stopped aesdsocket"
        fi
    ;;
    *) echo "Usage: $0 {start|stop}" ; exit 1 ;;
esac

exit 0