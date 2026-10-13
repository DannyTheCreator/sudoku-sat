# 上传 GitHub

推荐仓库名：`sudoku-sat`。

仓库简介可使用：

> A C11 DPLL SAT solver and Sudoku toolkit with standard/diagonal solving, unique puzzle generation, and a terminal game.

Topics 可使用 `c`、`sat-solver`、`dpll`、`sudoku`、`cmake`。

## 使用 Git 上传

先在 GitHub 创建空仓库，初始创建时不要额外生成 README 或 `.gitignore`。在本项目目录执行：

```bash
git init -b main
git add .
git status --short
git commit -m "Add Sudoku SAT solver and toolkit"
git remote add origin https://github.com/YOUR_USERNAME/sudoku-sat.git
git push -u origin main
```

将 `YOUR_USERNAME` 和仓库名换成自己的值。目录已初始化为 Git 仓库时省略 `git init`；已有首次提交时可直接添加远程并推送。构建输出、`.exe`、`.res`、本地编辑器配置和压缩包由 `.gitignore` 排除。

如果采用网页上传，上传项目内的文件与目录，并确保 `.github/`、`.gitignore` 等隐藏项目也包含在内。源码压缩包用于本地传输与备份；需要源码可浏览时，应解压后上传文件。

上传后在 Actions 页面检查跨平台编译与测试结果。当前工作流会在 push、pull request 或手动触发时执行，没有发布、部署或修改外部仓库的步骤。

## 发布前的项目资料

README、示例、测试和 CI 已包含在源码中。仓库目前未指定开源许可证；发布者决定授权方式后可添加 `LICENSE`。如原课程或合作项目要求署名，可将真实作者与课程信息补入 README。

本地验证记录见 [VALIDATION.md](VALIDATION.md)。云端 CI 首次运行前，不能将配置文件本身当成跨平台测试已经通过的证据。
