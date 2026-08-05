#!/usr/bin/env bash
# =============================================================================
# bench.sh —— mylib-proactor http_server 一键 QPS 压测
#
# 用法:
#   ./bench.sh [port] [server_threads] [duration] [connections] [wrk_threads]
#   默认:     8080        8              10s          1000        8
#
# 说明:
#   - 自动在后台启动 http_server, 压测结束后自动关闭
#   - 用 taskset 把服务端绑 0-7 号 vCPU(P核主线程), wrk 绑 16-31 号 vCPU(E核),
#     避免 13900HX 的 P/E 核调度抖动干扰结果
#     注意: 之前 wrk 绑 8-15 会和 server(0-7) 抢同一批 P 核超线程, 已修正
#   - 依次测 4 档响应体大小 (0/1KB/4KB/16KB), 输出汇总表
# =============================================================================
set -u

PORT="${1:-8080}"
SVR_THREADS="${2:-8}"
DURATION="${3:-10s}"
CONNS="${4:-1000}"
WRK_THREADS="${5:-8}"

# CPU 绑定: server 用 P 核主线程 0-7, wrk 用 E 核 16-31
SERVER_CPU="${SERVER_CPU:-0-7}"
WRK_CPU="${WRK_CPU:-16-31}"

# 定位可执行文件：优先 build/http_server，其次当前目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$SCRIPT_DIR/build/http_server" ]]; then
    SERVER_BIN="$SCRIPT_DIR/build/http_server"
elif [[ -x "$SCRIPT_DIR/http_server" ]]; then
    SERVER_BIN="$SCRIPT_DIR/http_server"
else
    echo "错误: 未找到 http_server 可执行文件 (build/http_server 或 ./http_server)"
    exit 1
fi

OUT_DIR="$SCRIPT_DIR/bench_results"
mkdir -p "$OUT_DIR"

# wrk 必须存在
command -v wrk >/dev/null 2>&1 || { echo "错误: 未找到 wrk, 请先安装"; exit 1; }

# 参数合法性
if [[ ! "$PORT" =~ ^[0-9]+$ ]] || [[ "$PORT" -lt 1 || "$PORT" -gt 65535 ]]; then
    echo "错误: 端口非法: $PORT"; exit 1
fi

# 清理可能残留的 http_server 进程
pkill -f "http_server" 2>/dev/null
sleep 0.5

echo "============================================================"
echo "  mylib-proactor HTTP 压测"
echo "  端口: $PORT | 服务端线程: $SVR_THREADS | wrk线程: $WRK_THREADS"
echo "  并发: $CONNS | 时长: $DURATION"
echo "============================================================"

# 服务器 PID 与结果收集
SERVER_PID=""
declare -A QPS_RESULTS

cleanup() {
    [[ -n "$SERVER_PID" ]] && kill "$SERVER_PID" 2>/dev/null
    pkill -f "./http_server" 2>/dev/null
}
trap cleanup EXIT

# 启动服务器 (绑定 0-7 号 vCPU)
start_server() {
    local body_size="$1"
    taskset -c "$SERVER_CPU" "$SERVER_BIN" -p "$PORT" -t "$SVR_THREADS" -b "$body_size" \
        > "$OUT_DIR/server_body${body_size}.log" 2>&1 &
    SERVER_PID=$!
    # 等待端口就绪
    for _ in $(seq 1 50); do
        if (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then
            exec 3>&- 3<&- 2>/dev/null
            return 0
        fi
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo "错误: 服务器进程已退出:"
            cat "$OUT_DIR/server_body${body_size}.log"
            return 1
        fi
        sleep 0.1
    done
    echo "错误: 服务器启动失败 (端口 $PORT 未就绪)"
    return 1
}

# 单次压测
run_one() {
    local body_size="$1"
    local label="$2"
    local conns="$3"

    echo ""
    echo "--- 测试: $label (body=${body_size}B, conns=${conns}) ---"

    if ! start_server "$body_size"; then return 1; fi

    # wrk 绑定 E 核(16-31); --latency 输出延迟分布
    local out
    out=$(taskset -c "$WRK_CPU" wrk -t"$WRK_THREADS" -c"$conns" -d"$DURATION" \
        --latency --timeout 5s \
        "http://127.0.0.1:${PORT}/" 2>&1)

    # wrk 原始输出落盘, 方便追溯
    echo "$out" > "$OUT_DIR/wrk_body${body_size}.log"

    # 提取 Requests/sec
    local qps
    qps=$(echo "$out" | awk '/Requests\/sec:/{print $2}')
    [[ -z "$qps" ]] && qps="N/A"
    QPS_RESULTS["$label"]="$qps"

    echo "$out" | grep -E "Requests/sec|Latency|Thread Stats" -A1 | head -12
    echo "  >>> $label QPS: $qps"

    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
    sleep 0.3
    return 0
}

# ---------- 测试序列 ----------
run_one 0     "空body"          "$CONNS"
run_one 1024  "1KB响应"         "$CONNS"
run_one 4096  "4KB响应"         "$((CONNS/2))"
run_one 16384 "16KB响应"        "$((CONNS/4))"

# ---------- 汇总 ----------
echo ""
echo "============================================================"
echo "  QPS 汇总 (并发=$CONNS, 时长=$DURATION, wrk线程=$WRK_THREADS)"
echo "============================================================"
printf "  %-12s %12s\n" "场景" "Requests/sec"
for k in "空body" "1KB响应" "4KB响应" "16KB响应"; do
    printf "  %-12s %12s\n" "$k" "${QPS_RESULTS[$k]:-N/A}"
done
echo "============================================================"
