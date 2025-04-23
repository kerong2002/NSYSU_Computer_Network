#!/bin/bash

# 殺掉已存在的 tmux 會話，避免重複啟動
tmux kill-session -t mysession 2>/dev/null

# 建立新的 tmux 會話
tmux new-session -d -s mysession

# 設定 tmux 的前綴鍵為 Ctrl + A，並啟用滑鼠滾動
tmux set-option -g prefix C-a
tmux bind-key C-a send-prefix
tmux set-option -g mouse on

# 第一次水平分割
tmux send-keys 'bash' C-m
tmux split-window -h  # 水平分割成左右兩部分

# 第二次水平分割
tmux select-pane -t 0  # 讓當前視窗選擇為最左邊的視窗
tmux split-window -h  # 再次在最左邊的視窗進行水平分割

# 執行 client、server、router
tmux send-keys -t 0 './server' C-m
tmux send-keys -t 1 './router' C-m
tmux send-keys -t 2 './client' C-m



# 等待 tmux 完成執行，並附加到 tmux 會話
tmux attach-session -t mysession

