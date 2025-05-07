#!/bin/bash

#!/bin/bash

# Function to check and kill process by port
kill_process_by_port() {
    PORT=$1
    RETRY_COUNT=3
    RETRY_INTERVAL=0.5

    for ((i=1; i<=RETRY_COUNT; i++)); do
        # 使用 lsof 檢查 TCP/UDP 連接
        PIDS=$(lsof -t -i:$PORT 2>/dev/null)

        # 如果沒有找到，用 fuser 檢查 TCP
        if [ -z "$PIDS" ]; then
            PIDS=$(fuser -n tcp $PORT 2>/dev/null)
        fi

        # 如果仍然沒有找到，用 fuser 檢查 UDP
        if [ -z "$PIDS" ]; then
            PIDS=$(fuser -n udp $PORT 2>/dev/null)
        fi

        # 如果找到 PID，逐一殺死
        if [ ! -z "$PIDS" ]; then
            echo "Attempt $i: Killing process(es) on port $PORT..."
            for PID in $PIDS; do
                echo " - PID: $PID"
                kill -9 $PID
            done

            # 等待一下再檢查是否成功清除
            sleep $RETRY_INTERVAL

        else
            echo "Attempt $i: No process found on port $PORT."
            break
        fi
    done

    # 最後再檢查一次是否還有殘留
    PIDS=$(lsof -t -i:$PORT 2>/dev/null)
    if [ -z "$PIDS" ]; then
        echo "Port $PORT is now clear."
    else
        echo "Warning: Port $PORT still in use after $RETRY_COUNT attempts."
    fi
}

# 測試用埠號列表
PORTS=(9000 9002 9003 9004 9010 9011 9012 9013)

# 遍歷所有埠號
for PORT in "${PORTS[@]}"; do
    kill_process_by_port $PORT
done

# Kill any existing tmux session to avoid duplicates
tmux kill-session -t mysession 2>/dev/null

# Create a new tmux session
tmux new-session -d -s mysession

# Set tmux options (prefix, mouse)
tmux set-option -g prefix C-a
tmux bind-key C-a send-prefix
tmux set-option -g mouse on

# First horizontal split
tmux send-keys 'bash' C-m
tmux split-window -h  # Split into left-right panes

# Second horizontal split on the left pane
tmux select-pane -t 0
tmux split-window -h  # Further split the left pane

# Execute client, server, router in different panes
tmux send-keys -t 0 './server' C-m
#sleep 0.1
tmux send-keys -t 1 './router' C-m
#sleep 0.1
tmux send-keys -t 2 './client' C-m


# Wait for tmux to finish execution and attach to the session
tmux attach-session -t mysession

