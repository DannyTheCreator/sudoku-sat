# Sudoku SAT

[English](README.md) | **简体中文**

[![Build and test](https://github.com/DannyTheCreator/sudoku-sat/actions/workflows/ci.yml/badge.svg)](https://github.com/DannyTheCreator/sudoku-sat/actions/workflows/ci.yml)

**用 C11 实现的 SAT 求解器与数独工具箱。** 支持普通数独、对角线数独（Sudoku X）、唯一解题目生成与终端游戏。

SAT 部分采用 DPLL、双监视文字单元传播和赋值栈回溯；数独求解通过 729 个布尔变量编码为 CNF。运行程序只依赖 C 标准库。

[快速开始](#快速开始) · [命令说明](#命令说明) · [文件格式](#文件格式) · [测试](#测试) · [实现原理](docs/ARCHITECTURE.zh-CN.md)

## 功能

| 功能 | 说明 |
| --- | --- |
| SAT 求解 | 读取 DIMACS CNF，输出 SAT / UNSAT、完整赋值和求解统计 |
| 数独求解 | 普通 9×9 数独；可启用两条主对角线约束 |
| CNF 导出 | 将题目导出为 DIMACS 文件，便于观察编码或交给其他求解器 |
| 唯一解生成 | 每次挖洞后计数到第二个解；相同模式、洞数与种子可复现 |
| 终端游戏 | 填写、清除、提示、查看答案、检查当前填写能否完成 |
| 工程检查 | CMake / CTest；GitHub Actions 配置覆盖 Windows、Linux、macOS |

## 快速开始

需要支持 C11 的编译器和 CMake 3.21+。Python 3.8+ 用于可选的命令行与随机公式验证，不参与程序运行。

### Windows / MinGW

在项目根目录打开 PowerShell。确保 `gcc`、`mingw32-make` 和 `cmake` 在 PATH 中：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\sudoku.exe solve examples\standard.txt --check-unique
```

### Windows / Visual Studio

在安装 C/C++ 工具链的 Visual Studio 开发者终端中执行：

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\sudoku.exe solve examples\standard.txt --check-unique
```

切换构建生成器时应使用新的构建目录，例如 `build-msvc`。

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/sudoku solve examples/standard.txt --check-unique
```

以下示例使用 `./build/sudoku`，Windows 下替换为对应的 `.exe` 路径即可。直接运行程序会显示交互菜单，支持保存求解结果、选择普通 / 对角线模式、保存生成题目并立即开始游戏；`--help` 显示完整参数。终端提示使用英文，便于在不同终端编码下运行。

## 命令说明

### 求解与导出

```bash
./build/sudoku sat examples/sat.cnf -o answer.res
./build/sudoku sat examples/unsat.cnf -o unsat.res
./build/sudoku solve examples/standard.txt --check-unique -o solved.txt
./build/sudoku solve examples/diagonal.txt --diagonal --check-unique
./build/sudoku encode examples/standard.txt puzzle.cnf
./build/sudoku sat puzzle.cnf -o puzzle.res
```

`--check-unique` 会报告 `unique` 或 `multiple`。可解但不唯一的题目仍返回成功。对角线模式要求两条长度为 9 的主对角线各包含一次 1–9；中心格同时受两条约束影响。

### 生成题目

```bash
./build/sudoku generate --holes 40 --seed 42 -o generated.txt --solution solution.txt
./build/sudoku generate --holes 50 --seed 42 --diagonal -o generated-x.txt
```

默认挖 40 个洞，默认种子取当前时间。`--holes` 范围为 0–64，`--seed` 范围为 0–4294967295，种子 0 映射到固定的非零内部状态。

生成器最多尝试 8 个完整盘面。成功时洞数与请求值一致且题目有唯一解；无法达到目标时返回退出码 2，不输出不满足条件的题目。洞数较高时可能失败，洞数也不等同于解题难度等级。

### 终端游戏

```bash
./build/sudoku play examples/standard.txt
./build/sudoku play examples/diagonal.txt --diagonal
```

行列均使用 **1–9**：

```text
set 1 3 4     填写第 1 行、第 3 列为 4
clear 1 3     清空该格
hint 1 3      按一个有效解填写该格
check         检查当前填写是否仍有解
answer        显示一个有效解
quit          退出
```

初始数字不可修改。与当前行、列、宫或对角线冲突的填写会被拒绝。对于存在多个解的输入，提示参考求解器返回的一个有效解，游戏完成判定接受任何满足约束的完整盘面。

## 文件格式

数独输入包含恰好 81 个格子，`1`–`9` 表示数字，`.` 或 `0` 表示空格，允许换行与空白：

```text
53..7....
6..195...
.98....6.
8...6...3
4..8.3..1
7...2...6
.6....28.
...419..5
....8..79
```

DIMACS CNF 需要 `p cnf <变量数> <子句数>` 头部。每条子句以 `0` 结束，可跨行；行首（允许缩进）的 `c` 表示注释。读取器检查子句数、变量范围、整数溢出和缺失终止符。重复文字会合并，永真子句会跳过；头部数量按原始输入子句检查。

SAT 结果格式：

```text
s 1
v 1 -2 3 0
t 0.000
```

`s 1` 表示可满足，`s 0` 表示不可满足；`v` 是完整模型并以 `0` 结束；`t` 为求解 CPU 时间，单位毫秒。未参与约束的变量在成功模型中取假。输出路径按原样使用，指定路径已存在时会覆盖；程序不会自动创建父目录。

| 退出码 | 含义 |
| --- | --- |
| 0 | 操作成功 |
| 1 | 参数、格式、冲突初值、文件读写或内部错误 |
| 2 | 正常求解判定无解，或唯一解生成未达到目标洞数 |

## 项目结构

```text
.
├── include/                 SAT 与数独公共接口
├── src/
│   ├── sat.c                DPLL、监视文字、赋值栈
│   ├── dimacs.c             CNF / 结果文件读写
│   ├── sudoku.c             约束编码、求解、唯一解计数与生成
│   └── main.c               命令行与终端交互
├── examples/                普通 / 对角线题目与 SAT / UNSAT 示例
├── tests/                   C 回归测试与 Python 穷举对照
├── docs/                    中英文实现原理说明
├── .github/                 持续集成与 issue / PR 模板
├── README.md                英文说明
└── README.zh-CN.md          中文说明
```

## 测试

`ctest` 运行 4 组 C 测试；找到 Python 后，还会运行命令行端到端测试与 250 组固定随机种子的 SAT 穷举对照。测试覆盖分支回溯、空公式 / 空子句、动态变量数量、异常 CNF、答案边界、两种数独模式、唯一性与种子复现。

可将警告视为错误：

```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Release -DWARNINGS_AS_ERRORS=ON
cmake --build build-strict --parallel
ctest --test-dir build-strict --output-on-failure
```

GCC / Clang 环境下的内存与未定义行为检查：

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

GitHub Actions 会在代码提交与拉取请求时运行 Windows、Linux、macOS 构建和测试，并在 Linux 下运行 AddressSanitizer 与 UndefinedBehaviorSanitizer。

## 实现与适用范围

数独求解将盘面编码为 CNF，由 DPLL 求解器返回模型。唯一性检查与题目生成采用位掩码和最少候选数优先（MRV）搜索。详细约束、回溯过程和接口约定见 [实现原理](docs/ARCHITECTURE.zh-CN.md)。

求解器面向算法学习和小中型 SAT 问题，尚未实现 CDCL、冲突学习或重启策略；大型困难实例可能耗时较长。
