# Linux 操作指令

## Output
![png](https://github.com/kerong2002/Computer_network/blob/main/hw2/output.png)

## 共享資料夾
在 Linux 中使用 VirtualBox 時，你可以執行以下指令來設定共享資料夾：

```bash
sudo usermod -aG vboxsf $(whoami)  # 將當前使用者加入 vboxsf 群組
sudo mount -t vboxsf Share /home/share  # 將共享資料夾掛載到 /home/share
```

# 使用 `tmux` 同時執行三個程式並分割視窗

這份筆記將引導你如何使用 `tmux` 同時執行三個程式（`client`, `server`, `router`），並將視窗自動分割成三個區域。

## 步驟 1: 創建 `start_tmux.sh` 腳本

首先，使用 `nano` 或你喜歡的文字編輯器創建一個名為 `start_tmux.sh` 的腳本檔案。

### 創建腳本
在終端中執行以下命令來創建腳本：

```bash
nano start_tmux.sh
```

### 腳本內容

將以下內容粘貼到 `start_tmux.sh` 中：

```bash
#!/bin/bash

# 函式：根據端口檢查並終止進程
kill_process_by_port() {
    PORT=$1  # 接收端口號作為參數
    PID=$(lsof -t -i:$PORT)  # 使用 lsof 查找指定端口的進程ID
    if [ ! -z "$PID" ]; then  # 如果找到進程ID
        echo "Killing process on port $PORT (PID: $PID)..."
        kill -9 $PID  # 強制終止該進程
    else
        echo "No process found on port $PORT."
    fi
}

# 關閉指定端口（9000、9002、9003、9004）上的進程
kill_process_by_port 9000
kill_process_by_port 9002
kill_process_by_port 9003
kill_process_by_port 9004

# 終止現有的 tmux 會話（如果有），避免重複創建
tmux kill-session -t mysession 2>/dev/null

# 創建一個新的 tmux 會話
tmux new-session -d -s mysession

# 設定 tmux 選項（快捷鍵前綴、啟用滑鼠支持）
tmux set-option -g prefix C-a  # 設定 tmux 快捷鍵前綴為 Ctrl-a
tmux bind-key C-a send-prefix  # 設定 Ctrl-a 為發送前綴的快捷鍵
tmux set-option -g mouse on  # 啟用滑鼠支持

# 第一個水平分割
tmux send-keys 'bash' C-m  # 在第一個窗格執行 bash
tmux split-window -h  # 將視窗分割為左右兩個窗格

# 在左側窗格中進行第二次水平分割
tmux select-pane -t 0  # 選擇左側窗格（第0個）
tmux split-window -h  # 進一步將左側窗格分割為兩個

# 在不同的窗格中執行 client、server 和 router
tmux send-keys -t 0 './server' C-m  # 在第一個窗格執行 server
tmux send-keys -t 1 './router' C-m  # 在第二個窗格執行 router
tmux send-keys -t 2 './client' C-m  # 在第三個窗格執行 client

# 等待 tmux 執行完成並附加到該會話
tmux attach-session -t mysession

```

### 給權限與執行
確保腳本有執行權限，並執行它：

```bash
chmod +x start_tmux.sh  # 給予執行權限
./start_tmux.sh  # 執行腳本
```

## 步驟 2: `Makefile` 自動編譯程式

`Makefile` 讓你可以自動編譯 `client`, `server`, `router` 三個程式。你可以根據以下步驟創建 `Makefile`，並簡化編譯過程。

### 創建 `Makefile`
在終端中執行以下命令來創建 `Makefile`：

```bash
nano Makefile
```

### `Makefile` 內容

```makefile
# 變數定義
CXX = g++              # C++ 編譯器
CC = gcc               # C 編譯器
CXXFLAGS = -pthread    # C++ 編譯選項
CFLAGS = -pthread      # C 編譯選項

# 預設目標
all: server client router

# 編譯 server
server: server.cpp
    g++ server.cpp -o server -pthread  # 編譯 server

# 編譯 client
client: client.cpp
    g++ client.cpp -o client -pthread  # 編譯 client

# 編譯 router
router: router.c
    gcc router.c -o router -pthread  # 編譯 router

# 清除編譯生成的檔案
clean:
    rm -f server client router  # 刪除已編譯的檔案
```

### 使用 `Makefile` 編譯程式

1. 編譯程式：
   ```bash
   make
   ```

2. 清除已編譯的檔案：
   ```bash
   make clean
   ```
