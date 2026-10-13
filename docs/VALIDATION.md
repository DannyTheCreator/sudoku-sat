# 本地验证记录

验证日期：2026-10-13。

## 环境与命令

- Windows x64。
- GCC 13.1.0（MinGW-w64）。
- CMake 3.31.0-rc1。
- Python 3.11.7。

在项目根目录执行：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DWARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

构建成功，严格警告检查通过。最终回归运行：**5 / 5 个 CTest 测试通过**，总耗时约 4.49 秒。耗时受机器与进程启动开销影响，不用于算法性能对比。

## 覆盖情况

| 测试 | 已验证内容 |
| --- | --- |
| `solver` | 空公式、空子句、失败分支撤销、两个分支皆失败、重复求解、6001 个变量、重复文字及永真子句 |
| `dimacs` | 跨行子句、缩进注释、读写往返、错误计数、越界文字、缺少终止符、无效头部、溢出与过长 token |
| `sudoku` | 已知普通题答案、变量 729 解码、唯一解、局部合法但全局无解的题目、冲突及越界数字、编码子句数 |
| `generator` | 两种模式 × 三个种子，恰好 40 个洞、唯一解、有效答案、SAT 与计数搜索结果一致、种子复现、0 洞及非法洞数 |
| `cli_and_differential` | 7 个 Python 测试方法，命令参数与退出码、文件管线、交互菜单、游戏操作、50 洞生成、原 CNF 兼容、250 组随机公式穷举对照 |

随机 SAT 对照使用固定种子 `20261013`，覆盖 0–7 个变量、0–18 条子句、0–6 个文字，包含重复文字、永真子句及空子句。逐案比较 SAT / UNSAT；可满足时额外检查返回模型是否满足原始子句。

原项目两份基础 CNF 的头部子句数分别为 7533、7551。兼容性测试加入题目的初值单元子句，并将求解出的完整盘面与新编码的求解结果对照。

## 验证边界

本记录仅表示上述 Windows / GCC 本地环境的实际结果。Linux、macOS、Windows / MSVC 和 Linux sanitizer 的 CI 已配置，但需要上传 GitHub 后查看实际运行结果。当前没有旧程序与新程序之间的性能基准测试，也没有任意规模 SAT 的形式化证明。
