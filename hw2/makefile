# 預設目標，當使用 `make` 不帶參數時會執行
all: server client router

# 編譯 server
server: server.cpp
	g++ server.cpp -o server -lpthread

# 編譯 client
client: client.cpp
	g++ client.cpp -o client -lpthread

# 編譯 router
router: router.c
	gcc router.c -o router -lpthread

# 清除編譯生成的檔案
clean:
	rm -f server client router
