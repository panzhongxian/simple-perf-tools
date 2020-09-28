## 监控CPU并gcore -- `get_procs_cpu_usage.cpp`

该工具会监控一批进程，并对指定的pid的CPU使用率超过指定值时，产生coredump文件以供分析。

几个默认参数：

- CPU使用率统计周期默认 500ms
- 不指定`gcore_pid`时，则只会统计不会core
- `core_cpu_up_threshold` 默认15%

### 编译

```bash
g++ -O3 -std=c++11 get_procs_cpu_usage.cpp -o get_procs_cpu_usage
```

### 使用

```
Usage:
    ./get_procs_cpu_usage proc_name [gcore_pid] [core_cpu_up_threshold]
```

Case 1: 监控所有启动参数中包含`spp_worker`的进程CPU使用情况：

```bash
./get_procs_cpu_usage spp_worker
```

Case 2: 要监控进程ID为42232的进程：

```bash
./get_procs_cpu_usage 42232
```

Case 3: 监控所有启动参数中包含`spp_worker`的进程CPU使用情况，当PID为42232的进程CPU使用率超过30%时产生core文件：

```bash
./get_procs_cpu_usage spp_worker 42232 30
```
