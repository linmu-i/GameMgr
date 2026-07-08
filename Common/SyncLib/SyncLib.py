#!/usr/bin/env python3
"""
依赖库同步工具（直接使用 local 字段，实时显示 Git 输出）
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from typing import List, Dict, Any

def run_git(args: List[str], cwd: str = None, timeout: int = 300) -> None:
    """
    执行 Git 命令，实时显示输出。
    - 对于进度行（包含 %），用 \r 在同一行更新
    - 对于普通行，换行打印
    """
    cmd_str = ' '.join(args)
    print(f"  [执行] {cmd_str}")

    try:
        with subprocess.Popen(
            args,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        ) as proc:
            start_time = time.time()
            last_was_progress = False  # 上一行是否进度行
            for line in proc.stdout:
                # 检查超时
                if time.time() - start_time > timeout:
                    proc.kill()
                    raise subprocess.TimeoutExpired(cmd_str, timeout)

                line = line.rstrip('\n\r')  # 去掉换行，保留可能存在的内部 \r
                # 判断是否为进度行（包含 % 通常就是下载、检出进度）
                if '%' in line:
                    # 用回车覆盖当前行，不换行
                    print(f"\r    {line}", end='', flush=True)
                    last_was_progress = True
                else:
                    # 普通行：如果之前是进度行，先换行
                    if last_was_progress:
                        print()  # 结束进度行
                    print(f"    {line}")
                    last_was_progress = False

            # 循环结束后，如果最后一行是进度行，补一个换行
            if last_was_progress:
                print()

            retcode = proc.wait()
            if retcode != 0:
                raise subprocess.CalledProcessError(retcode, cmd_str)

    except subprocess.TimeoutExpired:
        print(f"\n错误：Git 命令超时 ({timeout}秒): {cmd_str}", file=sys.stderr)
        raise
    except subprocess.CalledProcessError as e:
        print(f"错误：Git 命令退出码 {e.returncode}: {cmd_str}", file=sys.stderr)
        raise
    except KeyboardInterrupt:
        print("\n用户中断操作", file=sys.stderr)
        try:
            proc.kill()
        except:
            pass
        sys.exit(1)

def is_git_repo(path: str) -> bool:
    """检查 path 是否为一个有效的 Git 仓库"""
    return os.path.isdir(os.path.join(path, '.git'))

def sync_task(task: Dict[str, Any]) -> None:
    remote = task["remote"]
    local = os.path.abspath(task["local"])
    branch = task.get("branch") or None
    whitelist = task.get("whitelist", [])
    blacklist = task.get("blacklist", [])

    print(f"任务: {local}")

    # 构造稀疏检出模式
    if not whitelist:
        patterns = ["*"] + ["!" + b for b in blacklist]
    else:
        patterns = whitelist + ["!" + b for b in blacklist]

    # 增量更新：如果本地已是 Git 仓库，直接更新
    if is_git_repo(local):
        print("  发现已有仓库，执行增量更新...")
        # 1. 确保远程地址正确
        run_git(["git", "-C", local, "remote", "set-url", "origin", remote])
        # 2. 拉取最新提交（保持浅克隆 --depth 1）
        run_git(["git", "-C", local, "fetch", "--depth", "1"])
        # 3. 重新应用稀疏检出（应对白名单/黑名单变化）
        print(f"  稀疏检出模式: {patterns}")
        run_git(["git", "-C", local, "sparse-checkout", "set", "--no-cone"] + patterns)
        # 4. 强制切换到指定分支并更新工作目录
        checkout_branch = branch or "master"   # 未指定分支时默认 master，建议始终在配置中填写
        run_git(["git", "-C", local, "checkout", "-B", checkout_branch,
                 f"origin/{checkout_branch}"])
        print(f"  增量更新完成: {local}\n")
        return

    # 全量同步：目录不存在或不是 Git 仓库，则删除重建
    if os.path.exists(local):
        print(f"  删除已存在的目标路径: {local}")
        remove_directory(local)

    os.makedirs(os.path.dirname(local), exist_ok=True)

    # 克隆
    clone_cmd = ["git", "clone", "--no-checkout", "--depth", "1", "--progress"]
    if branch:
        clone_cmd += ["--branch", branch]
    clone_cmd += [remote, local]
    run_git(clone_cmd)

    # 稀疏检出
    print(f"  稀疏检出模式: {patterns}")
    run_git(["git", "-C", local, "sparse-checkout", "set", "--no-cone"] + patterns)
    run_git(["git", "-C", local, "checkout", "--progress"])
    print(f"  同步完成: {local}\n")

def main():
    parser = argparse.ArgumentParser(description="依赖库同步工具")
    parser.add_argument("--config", default="SyncLibConfig.json")
    parser.add_argument("--timeout", type=int, default=300,
                        help="Git 命令超时时间（秒），默认300")
    args = parser.parse_args()

    try:
        with open(args.config, "r", encoding="utf-8") as f:
            config = json.load(f)
    except FileNotFoundError:
        sys.exit(f"配置文件未找到: {args.config}")
    except json.JSONDecodeError as e:
        sys.exit(f"JSON 解析错误: {e}")

    tasks = config.get("tasks", [])
    if not tasks:
        print("没有任务。")
        return

    for i, t in enumerate(tasks, 1):
        print(f"--- 任务 {i}/{len(tasks)} ---")
        try:
            sync_task(t)
        except Exception as e:
            print(f"任务执行失败: {e}", file=sys.stderr)
            sys.exit(1)
    print("所有任务执行完毕。")

if __name__ == "__main__":
    main()