#!/bin/bash

# 殺掉已存在的 tmux 會話，避免重複啟動
tmux kill-session -t mysession 2>/dev/null

# 建立新的 tmux 會話
tmux new-session -d -s mysession

# 設定 tmux 的前綴鍵為 Ctrl + A，並啟用滑鼠滾動
tmux set-option -g prefix C-a
tmux bind-key C-a send-prefix
tmux set-option -g mouse on

# 在 tmux 視窗內分割三個區塊
tmux send-keys 'bash' C-m
tmux split-window -h
tmux split-window -v

# 執行 client、server、router
tmux send-keys -t 0 './client' C-m
tmux send-keys -t 1 './server' C-m
tmux send-keys -t 2 './router' C-m

# 附加到 tmux 會話
tmux attach-session -t mysession

