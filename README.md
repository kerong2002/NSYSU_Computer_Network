# Computer_network
# Linux 操作指令

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

# 刪除現有會話，如果有的話
tmux kill-session -t mysession 2>/dev/null

# 創建一個新的 tmux 會話，並分割視窗
tmux new-session -d -s mysession \
    'bash' \; \
    split-window -h \; \
    split-window -v \; \
    send-keys -t 0 './client' C-m \; \
    send-keys -t 1 './server' C-m \; \
    send-keys -t 2 './router' C-m \; \
    attach-session -t mysession
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
