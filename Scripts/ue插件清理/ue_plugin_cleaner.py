#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UE插件清理工具 - 清理PDB等冗余文件
版本: v3.0
功能: 支持压缩包处理、直接删除Source文件夹
作者: Assistant
日期: 2025
"""

import os
import sys
import glob
import shutil
import argparse
import zipfile
import tempfile
import time
import threading
from pathlib import Path
from datetime import datetime
import collections

# 颜色代码
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

# 支持的颜色终端检测
def use_colors():
    """检测是否支持颜色输出"""
    # 在Windows上启用颜色（但避免emoji）
    return sys.stdout.isatty()

def color_text(text, color):
    """给文本添加颜色"""
    if use_colors():
        return f"{color}{text}{Colors.ENDC}"
    return text

def format_size(size_bytes):
    """格式化文件大小"""
    if size_bytes == 0:
        return "0B"
    size_name = ("B", "KB", "MB", "GB", "TB")
    i = 0
    while size_bytes >= 1024 and i < len(size_name) - 1:
        size_bytes /= 1024.0
        i += 1
    return f"{size_bytes:.2f} {size_name[i]}"

def get_terminal_height():
    """获取终端高度"""
    try:
        return os.get_terminal_size().lines
    except:
        return 25  # 默认值

def show_progress_bar(current, total, prefix='', width=40):
    """显示进度条"""
    percent = current / total
    arrow = '=' * int(width * percent) + '>' if percent < 1 else '=' * int(width * percent)
    spaces = ' ' * (width - int(width * percent))
    return f'\r{prefix} [{arrow}{spaces}] {percent:.0%}'

class PluginCleaner:
    def __init__(self, plugin_path, verbose=False):
        self.plugin_path = Path(plugin_path)
        self.verbose = verbose
        self.files_to_delete = []
        self.dirs_to_delete = []
        self.total_size = 0
        
        # 定义要清理的文件和目录
        self.target_patterns = [
            # PDB调试符号文件
            "**/*.pdb",
            # 中间文件
            "**/Intermediate/**",
            # 派生数据缓存
            "**/DerivedDataCache/**",
            # 编译日志
            "**/*.log",
            # 临时文件
            "**/*.tmp",
            "**/*.temp",
            # 编译器生成的临时文件
            "**/*.obj",
            "**/*.tlog",
            "**/*.lastbuildstate",
            "**/*.buildlog",
            "**/*.ipch",
            # Visual Studio临时文件
            "**/*.vsidx",
            "**/*.VC.db",
            "**/*.VC.VC.opendb",
            # 代码覆盖率文件
            "**/*.pgc",
            "**/*.pgd",
            "**/*.pgd",
            # Intel TBB文件
            "**/*_tbb*",
            # 其他可能的临时文件
            "**/*.ilk",
            "**/*.exp",
            "**/*.lib",
            "**/*.pch",
            # Source文件夹（整个删除）
            "**/Source/**",
        ]
        
        # 需要保留的关键文件模式
        self.keep_patterns = [
            "**/*.dll",
            "**/*.exe",
            "**/*.so",
            "**/*.dylib",
            "**/*.uplugin",
            "**/*.uplugin-desc",
            "**/*.umap",
            "**/*.uasset",
            "**/*.ini",
            "**/*.config",
            "**/*.json",
            "**/*.xml",
            "**/*.h",
            "**/*.cpp",
            "**/*.c",
            "**/*.cs",
            "**/*.py",
            "**/*.md",
            "**/*.txt",
            "**/*.png",
            "**/*.jpg",
            "**/*.jpeg",
            "**/*.gif",
            "**/*.svg",
            "**/*.ico",
        ]

    def is_zip_file(self):
        """检查输入路径是否为zip文件"""
        return self.plugin_path.suffix.lower() in ['.zip']

    def process_zip_file(self, need_confirm=True):
        """处理zip文件：解压、清理、重新打包"""
        print(f"检测到zip文件: {self.plugin_path.name}")
        print("正在解压到临时目录...")

        # 使用脚本同目录的临时目录，避免C盘权限问题
        script_dir = Path(__file__).parent
        temp_dir = script_dir / f"temp_{int(time.time())}_{os.getpid()}"

        try:
            # 创建临时目录
            os.makedirs(temp_dir, exist_ok=True)
            print(f"临时目录: {temp_dir}")

            # 解压zip文件
            try:
                with zipfile.ZipFile(self.plugin_path, 'r') as zip_ref:
                    zip_ref.extractall(temp_dir)
                print(f"解压完成")
            except zipfile.BadZipFile:
                print(f"错误: {self.plugin_path} 不是有效的zip文件")
                return False

            # 保存原始路径
            original_plugin_path = self.plugin_path
            temp_path = Path(temp_dir)

            # 重新扫描（只扫描一次，避免递归）
            self.files_to_delete = []
            self.dirs_to_delete = []
            self.total_size = 0

            # 检查是否找到插件
            plugin_files = list(temp_path.rglob('*.uplugin'))
            if plugin_files:
                print(f"找到插件文件: {plugin_files[0].name}")
            else:
                print("警告: 未找到插件描述文件，继续扫描...")

            # 扫描目标文件和目录
            total_items = sum(1 for _ in temp_path.rglob('*'))
            processed = 0
            print("\n扫描文件...")

            for pattern in self.target_patterns:
                for item in temp_path.glob(pattern):
                    if item.is_file():
                        # 检查是否应该保留
                        should_keep = False
                        for keep_pattern in self.keep_patterns:
                            if item.match(keep_pattern):
                                should_keep = True
                                break

                        if not should_keep:
                            try:
                                size = item.stat().st_size
                                self.files_to_delete.append((item, size))
                            except (OSError, FileNotFoundError):
                                pass

                    elif item.is_dir():
                        try:
                            dir_size = self._calculate_dir_size(item)
                            self.dirs_to_delete.append((item, dir_size))
                        except (OSError, FileNotFoundError):
                            pass

                    processed += 1
                    if processed % 10 == 0 or processed == total_items:
                        print(show_progress_bar(processed, total_items, "扫描进度"), end='')

            print()  # 换行

            # 按大小排序文件
            self.files_to_delete.sort(key=lambda x: x[1], reverse=True)

            # 计算总大小
            self.total_size = self._calculate_total_size()

            # 创建显示用的临时清理器
            display_cleaner = PluginCleaner(Path("."), self.verbose)
            display_cleaner.files_to_delete = self.files_to_delete
            display_cleaner.dirs_to_delete = self.dirs_to_delete
            display_cleaner.total_size = self.total_size
            display_cleaner.plugin_path = Path("__zip_display__")
            # 手动创建显示用路径结构
            display_cleaner.plugin_path = Path(original_plugin_path.name)

            # 显示预览
            display_cleaner.preview_plugin_cleanup()

            # 确认删除（批量模式下不需要再次确认）
            if need_confirm:
                print(color_text("\n[警告] 此操作将永久删除上述文件和目录!", Colors.WARNING))
                confirm = input("确认删除? (y/N): ").strip().lower()

                if confirm != 'y':
                    print("操作已取消。")
                    self._cleanup_temp_dir(temp_path)
                    return False
            else:
                # 批量模式下也需要清理临时目录
                pass

            # 执行清理
            print("\n开始删除文件和目录...")

            # 先删除文件
            file_count = len(self.files_to_delete)
            for i, (file_path, _) in enumerate(self.files_to_delete, 1):
                if file_path.exists():
                    try:
                        if self.verbose:
                            print(f"删除文件: {str(file_path)}")
                        file_path.unlink()
                    except OSError as e:
                        print(f"删除文件失败: {str(file_path)} - {e}")
                if i % 5 == 0 or i == file_count:
                    print(show_progress_bar(i, file_count, "删除文件"), end='')

            print()  # 换行

            # 删除目录（按层级排序，避免重复删除子目录）
            # 按路径长度排序，短的路径（父目录）先删除
            sorted_dirs = sorted(self.dirs_to_delete, key=lambda x: len(str(x[0])))
            dir_count = len(sorted_dirs)

            for i, (dir_path, _) in enumerate(sorted_dirs, 1):
                if dir_path.exists():
                    try:
                        if self.verbose:
                            print(f"删除目录: {str(dir_path)}")
                        shutil.rmtree(dir_path)
                    except OSError as e:
                        # 忽略目录已不存在的错误
                        if "系统找不到指定的路径" not in str(e):
                            print(f"删除目录失败: {str(dir_path)} - {e}")
                if i % 3 == 0 or i == dir_count:
                    print(show_progress_bar(i, dir_count, "删除目录"), end='')

            print()  # 换行

            # 重新压缩
            print(f"\n正在重新打包...")
            try:
                # 检查目标文件是否存在
                if original_plugin_path.exists():
                    try:
                        # 尝试修改文件为可写
                        original_plugin_path.chmod(0o666)
                        os.remove(original_plugin_path)
                    except PermissionError:
                        print(f"错误: 无法覆盖目标文件 {original_plugin_path}")
                        print("请检查：")
                        print("  1. 文件是否被其他程序占用")
                        print("  2. 是否有足够的权限")
                        print("  3. 文件路径是否有效")
                        print("\n临时目录已清理，压缩包内容将丢失。请手动处理。")
                        self._cleanup_temp_dir(temp_path)
                        return False

                file_list = list(temp_path.rglob('*'))
                total_files = len(file_list)

                with zipfile.ZipFile(original_plugin_path, 'w', zipfile.ZIP_DEFLATED) as zip_ref:
                    for i, file_path in enumerate(file_list, 1):
                        if file_path.is_file():
                            # 计算相对路径
                            arcname = file_path.relative_to(temp_path)
                            zip_ref.write(file_path, arcname)
                        if i % 5 == 0 or i == total_files:
                            print(show_progress_bar(i, total_files, "打包进度"), end='')

                print()  # 换行
                print(f"重新打包完成: {original_plugin_path}")
            except PermissionError as e:
                print(f"权限错误: 无法写入文件 {original_plugin_path}")
                print(f"详细信息: {e}")
                print("\n请尝试：")
                print("  1. 以管理员身份运行")
                print("  2. 检查文件是否被占用")
                print("  3. 确认目标路径可写")
                self._cleanup_temp_dir(temp_path)
                return False
            except Exception as e:
                print(f"重新打包失败: {e}")
                self._cleanup_temp_dir(temp_path)
                return False

            return True

        except Exception as e:
            print(f"处理失败: {e}")
            return False
        finally:
            # 确保无论如何都尝试清理临时目录
            if temp_dir and Path(temp_dir).exists():
                self._cleanup_temp_dir(temp_dir)

    def _cleanup_temp_dir(self, temp_path):
        """清理临时目录（更强力的清理）"""
        if not temp_path or not Path(temp_path).exists():
            return

        try:
            # 先尝试修改权限
            for root, dirs, files in os.walk(temp_path):
                for d in dirs:
                    dir_path = os.path.join(root, d)
                    try:
                        os.chmod(dir_path, 0o777)
                    except:
                        pass
                for f in files:
                    file_path = os.path.join(root, f)
                    try:
                        os.chmod(file_path, 0o777)
                    except:
                        pass

            # 强制删除
            shutil.rmtree(temp_path, ignore_errors=False)
            print(f"临时目录已清理: {Path(temp_path).name}")
        except Exception as e:
            try:
                # 如果还是失败，尝试忽略错误删除
                shutil.rmtree(temp_path, ignore_errors=True)
                print(f"临时目录已清理 (忽略错误): {Path(temp_path).name}")
            except:
                print(f"警告: 无法清理临时目录 {Path(temp_path).name}，请手动删除")


    def scan_plugin(self, need_confirm=True):
        """扫描插件目录，找出需要清理的文件"""
        if not self.plugin_path.exists():
            print(f"错误: 插件路径不存在: {self.plugin_path}")
            return False

        # 处理zip文件
        if self.is_zip_file():
            return self.process_zip_file(need_confirm=need_confirm)

        if not self.plugin_path.is_dir():
            print(f"错误: 指定路径不是目录: {self.plugin_path}")
            return False
            
        # 检查是否是UE插件目录
        plugin_file = self.plugin_path / f"{self.plugin_path.name}.uplugin"
        if not plugin_file.exists():
            print(f"警告: 未找到插件描述文件 {plugin_file}")
            print("这可能不是一个标准的UE插件目录，继续扫描...")
        
        # 扫描目标文件和目录
        for pattern in self.target_patterns:
            for item in self.plugin_path.glob(pattern):
                if item.is_file():
                    # 检查是否应该保留
                    should_keep = False
                    for keep_pattern in self.keep_patterns:
                        if item.match(keep_pattern):
                            should_keep = True
                            break
                    
                    if not should_keep:
                        try:
                            size = item.stat().st_size
                            self.files_to_delete.append((item, size))
                        except (OSError, FileNotFoundError):
                            # 文件可能已被删除或无法访问
                            pass
                            
                elif item.is_dir():
                    # 检查目录是否为空或只包含临时文件
                    try:
                        dir_size = self._calculate_dir_size(item)
                        self.dirs_to_delete.append((item, dir_size))
                        # 注意：这里不直接累加dir_size到total_size，因为会在后面重新计算
                    except (OSError, FileNotFoundError):
                        # 目录可能已被删除或无法访问
                        pass
        
        # 按大小排序文件
        self.files_to_delete.sort(key=lambda x: x[1], reverse=True)
        
        # 重新计算总大小，避免子目录重复计算
        processed_dirs = set()
        self.total_size = 0
        
        # 计算目录大小（只计算顶级目录）
        for dir_path, dir_size in self.dirs_to_delete:
            dir_path_str = str(dir_path)
            
            # 检查是否是其他目录的子目录
            is_subdir = False
            for processed_dir in processed_dirs:
                if dir_path_str.startswith(processed_dir + os.sep) or dir_path_str.startswith(processed_dir + '/'):
                    is_subdir = True
                    break
            
            # 如果不是子目录，累加到总大小
            if not is_subdir:
                self.total_size += dir_size
                processed_dirs.add(dir_path_str)
        
        # 计算不在目录中的独立文件大小
        dir_paths = set()
        for dir_path, _ in self.dirs_to_delete:
            dir_paths.add(str(dir_path))
        
        for file_path, file_size in self.files_to_delete:
            # 检查文件是否在任何要删除的目录中
            file_in_dir = False
            for dir_path in dir_paths:
                if str(file_path).startswith(dir_path):
                    file_in_dir = True
                    break
            
            # 只统计不在目录中的独立文件
            if not file_in_dir:
                self.total_size += file_size
        
        return True

    def _scan_only(self):
        """仅扫描插件目录，不执行清理（用于批量处理前统计）"""
        if not self.plugin_path.exists():
            return False

        if not self.plugin_path.is_dir():
            return False

        # 检查是否是UE插件目录
        plugin_file = self.plugin_path / f"{self.plugin_path.name}.uplugin"
        if not plugin_file.exists():
            # 继续扫描，即使没有插件文件
            pass

        # 扫描目标文件和目录
        for pattern in self.target_patterns:
            for item in self.plugin_path.glob(pattern):
                if item.is_file():
                    # 检查是否应该保留
                    should_keep = False
                    for keep_pattern in self.keep_patterns:
                        if item.match(keep_pattern):
                            should_keep = True
                            break

                    if not should_keep:
                        try:
                            size = item.stat().st_size
                            self.files_to_delete.append((item, size))
                        except (OSError, FileNotFoundError):
                            pass

                elif item.is_dir():
                    try:
                        dir_size = self._calculate_dir_size(item)
                        self.dirs_to_delete.append((item, dir_size))
                    except (OSError, FileNotFoundError):
                        pass

        # 按大小排序文件
        self.files_to_delete.sort(key=lambda x: x[1], reverse=True)

        # 计算总大小（避免子目录重复计算）
        processed_dirs = set()
        self.total_size = 0

        for dir_path, dir_size in self.dirs_to_delete:
            dir_path_str = str(dir_path)

            # 检查是否是其他目录的子目录
            is_subdir = False
            for processed_dir in processed_dirs:
                if dir_path_str.startswith(processed_dir + os.sep) or dir_path_str.startswith(processed_dir + '/'):
                    is_subdir = True
                    break

            # 如果不是子目录，累加到总大小
            if not is_subdir:
                self.total_size += dir_size
                processed_dirs.add(dir_path_str)

        # 计算不在目录中的独立文件大小
        dir_paths = set()
        for dir_path, _ in self.dirs_to_delete:
            dir_paths.add(str(dir_path))

        for file_path, file_size in self.files_to_delete:
            # 检查文件是否在任何要删除的目录中
            file_in_dir = False
            for dir_path in dir_paths:
                if str(file_path).startswith(dir_path):
                    file_in_dir = True
                    break

            # 只统计不在目录中的独立文件
            if not file_in_dir:
                self.total_size += file_size

        return True

    def _calculate_dir_size(self, directory):
        """计算目录大小"""
        total_size = 0
        try:
            for dirpath, dirnames, filenames in os.walk(directory):
                for filename in filenames:
                    filepath = os.path.join(dirpath, filename)
                    try:
                        total_size += os.path.getsize(filepath)
                    except (OSError, FileNotFoundError):
                        # 文件可能已被删除或无法访问
                        pass
        except (OSError, FileNotFoundError):
            # 目录可能已被删除或无法访问
            pass
        return total_size

    def _calculate_total_size(self):
        """计算总大小（避免重复计算子目录）"""
        total_size = 0
        processed_dirs = set()

        # 计算目录大小（只计算顶级目录）
        for dir_path, dir_size in self.dirs_to_delete:
            dir_path_str = str(dir_path)

            # 检查是否是其他目录的子目录
            is_subdir = False
            for processed_dir in processed_dirs:
                if dir_path_str.startswith(processed_dir + os.sep) or dir_path_str.startswith(processed_dir + '/'):
                    is_subdir = True
                    break

            # 如果不是子目录，累加到总大小
            if not is_subdir:
                total_size += dir_size
                processed_dirs.add(dir_path_str)

        # 计算不在目录中的独立文件大小
        dir_paths = set()
        for dir_path, _ in self.dirs_to_delete:
            dir_paths.add(str(dir_path))

        for file_path, file_size in self.files_to_delete:
            # 检查文件是否在任何要删除的目录中
            file_in_dir = False
            for dir_path in dir_paths:
                if str(file_path).startswith(dir_path):
                    file_in_dir = True
                    break

            # 只统计不在目录中的独立文件
            if not file_in_dir:
                total_size += file_size

        return total_size
    
    def _get_file_type_stats(self):
        """获取文件类型统计"""
        type_stats = collections.defaultdict(lambda: {'count': 0, 'size': 0})

        # 创建一个包含所有目录路径的集合，用于检查文件是否在目录中
        dir_paths = set()
        for dir_path, _ in self.dirs_to_delete:
            dir_paths.add(str(dir_path))

        for file_path, file_size in self.files_to_delete:
            # 检查文件是否在任何要删除的目录中
            file_in_dir = False
            for dir_path in dir_paths:
                if str(file_path).startswith(dir_path):
                    file_in_dir = True
                    break

            # 只统计不在目录中的独立文件
            if not file_in_dir:
                ext = file_path.suffix.lower()
                type_stats[ext]['count'] += 1
                type_stats[ext]['size'] += file_size

        # 按大小排序
        sorted_types = sorted(type_stats.items(), key=lambda x: x[1]['size'], reverse=True)
        return sorted_types
    
    def _get_directory_structure(self):
        """获取目录结构，用于更详细地显示"""
        structure = {}

        # 首先构建目录结构，但不计算大小
        for dir_path, dir_size in self.dirs_to_delete:
            try:
                # 获取相对于插件根目录的路径
                rel_path = dir_path.relative_to(self.plugin_path)
            except ValueError:
                # 如果无法计算相对路径（如压缩包中的临时路径），跳过
                continue
            parts = rel_path.parts

            # 构建嵌套字典结构
            current = structure
            for part in parts[:-1]:
                if part not in current:
                    current[part] = {'_size': 0, '_dirs': {}}
                current = current[part]['_dirs']

            # 添加最后一个部分（目录名）
            if parts[-1] not in current:
                current[parts[-1]] = {'_size': dir_size, '_dirs': {}}
            else:
                # 如果目录已存在，只更新大小（不累加）
                current[parts[-1]]['_size'] = dir_size

        # 然后递归计算每个父目录的大小（只计算一次）
        def calculate_total_sizes(node):
            """递归计算节点及其子节点的总大小"""
            if '_dirs' not in node or not node['_dirs']:
                # 叶子节点，直接返回自己的大小
                return node['_size']

            # 非叶子节点，计算所有子节点的总大小
            total_size = 0
            for child_name, child_node in node['_dirs'].items():
                total_size += calculate_total_sizes(child_node)

            # 更新节点大小为子节点总大小
            node['_size'] = total_size
            return total_size

        # 计算所有节点的总大小
        for root_name, root_node in structure.items():
            calculate_total_sizes(root_node)

        return structure
    
    def _format_directory_tree(self, structure, prefix='', is_last=True, max_depth=3, current_depth=0):
        """格式化目录树显示"""
        lines = []
        
        # 限制递归深度
        if current_depth >= max_depth:
            return lines
        
        # 按大小排序目录
        sorted_dirs = sorted(structure.items(), key=lambda x: x[1]['_size'], reverse=True)
        
        for i, (name, data) in enumerate(sorted_dirs):
            is_last_dir = i == len(sorted_dirs) - 1
            connector = '└─' if is_last_dir else '├─'
            lines.append(f"{prefix}{connector} {name}/ ({format_size(data['_size'])})")
            
            # 递归处理子目录
            if data['_dirs'] and current_depth < max_depth - 1:
                extension = '    ' if is_last_dir else '│   '
                lines.extend(self._format_directory_tree(
                    data['_dirs'], 
                    prefix + extension, 
                    is_last_dir, 
                    max_depth, 
                    current_depth + 1
                ))
        
        return lines
    
    def _categorize_directories(self):
        """将目录按功能分类"""
        categories = {
            'Source源码': {'dirs': [], 'size': 0, 'count': 0},
            '编译缓存': {'dirs': [], 'size': 0, 'count': 0},
            '派生数据缓存': {'dirs': [], 'size': 0, 'count': 0},
            '二进制文件': {'dirs': [], 'size': 0, 'count': 0},
            '配置文件': {'dirs': [], 'size': 0, 'count': 0},
            '资源文件': {'dirs': [], 'size': 0, 'count': 0},
            '文档目录': {'dirs': [], 'size': 0, 'count': 0},
            '测试目录': {'dirs': [], 'size': 0, 'count': 0},
            '临时文件': {'dirs': [], 'size': 0, 'count': 0},
            '调试文件': {'dirs': [], 'size': 0, 'count': 0},
            '日志文件': {'dirs': [], 'size': 0, 'count': 0},
            'IDE缓存': {'dirs': [], 'size': 0, 'count': 0},
            '其他目录': {'dirs': [], 'size': 0, 'count': 0}
        }

        # 创建一个集合，用于跟踪已处理的目录路径
        processed_dirs = set()

        for dir_path, dir_size in self.dirs_to_delete:
            dir_path_str = str(dir_path)

            # 检查是否是其他目录的子目录
            is_subdir = False
            for processed_dir in processed_dirs:
                if dir_path_str.startswith(processed_dir + os.sep) or dir_path_str.startswith(processed_dir + '/'):
                    is_subdir = True
                    break

            # 如果是子目录，跳过处理，避免重复计算
            if is_subdir:
                continue

            # 标记此目录为已处理
            processed_dirs.add(dir_path_str)

            # 获取目录名（对于显示，尝试使用相对路径，否则用完整路径）
            try:
                if str(dir_path).startswith(str(self.plugin_path)):
                    dir_name = dir_path.name
                else:
                    # 可能是临时路径，直接使用目录名
                    dir_name = dir_path.name
            except (ValueError, AttributeError):
                dir_name = dir_path.name

            dir_name_lower = dir_name.lower()
            full_path_str = str(dir_path).lower()

            # 根据目录名称和路径进行分类（按优先级排序）
            # Source目录优先
            if 'source' in full_path_str or dir_name_lower in ['source', 'src', 'code']:
                categories['Source源码']['dirs'].append(dir_path)
                categories['Source源码']['size'] += dir_size
                categories['Source源码']['count'] += 1
            elif 'intermediate' in full_path_str or 'build' in full_path_str:
                categories['编译缓存']['dirs'].append(dir_path)
                categories['编译缓存']['size'] += dir_size
                categories['编译缓存']['count'] += 1
            elif 'deriveddatacache' in full_path_str or 'ddc' in full_path_str:
                categories['派生数据缓存']['dirs'].append(dir_path)
                categories['派生数据缓存']['size'] += dir_size
                categories['派生数据缓存']['count'] += 1
            elif 'binaries' in full_path_str or dir_name_lower in ['binaries', 'bin']:
                categories['二进制文件']['dirs'].append(dir_path)
                categories['二进制文件']['size'] += dir_size
                categories['二进制文件']['count'] += 1
            elif dir_name_lower in ['config', 'configs', 'configuration', 'configurations']:
                categories['配置文件']['dirs'].append(dir_path)
                categories['配置文件']['size'] += dir_size
                categories['配置文件']['count'] += 1
            elif dir_name_lower in ['resources', 'resource', 'assets', 'asset', 'textures', 'materials', 'shaders', 'meshes', 'audio', 'sounds']:
                categories['资源文件']['dirs'].append(dir_path)
                categories['资源文件']['size'] += dir_size
                categories['资源文件']['count'] += 1
            elif dir_name_lower in ['docs', 'documentation', 'doc', 'manual', 'readme', 'help']:
                categories['文档目录']['dirs'].append(dir_path)
                categories['文档目录']['size'] += dir_size
                categories['文档目录']['count'] += 1
            elif dir_name_lower in ['test', 'tests', 'testing', 'unittest', 'unittests', 'qa', 'validation']:
                categories['测试目录']['dirs'].append(dir_path)
                categories['测试目录']['size'] += dir_size
                categories['测试目录']['count'] += 1
            elif dir_name_lower in ['temp', 'tmp'] or 'temp' in dir_name_lower:
                categories['临时文件']['dirs'].append(dir_path)
                categories['临时文件']['size'] += dir_size
                categories['临时文件']['count'] += 1
            elif dir_name_lower in ['debug', 'logs', 'log'] or 'debug' in dir_name_lower or 'log' in dir_name_lower:
                if 'log' in dir_name_lower:
                    categories['日志文件']['dirs'].append(dir_path)
                    categories['日志文件']['size'] += dir_size
                    categories['日志文件']['count'] += 1
                else:
                    categories['调试文件']['dirs'].append(dir_path)
                    categories['调试文件']['size'] += dir_size
                    categories['调试文件']['count'] += 1
            elif '.vs' in full_path_str or '__pycache__' in full_path_str or 'cache' in dir_name_lower:
                categories['IDE缓存']['dirs'].append(dir_path)
                categories['IDE缓存']['size'] += dir_size
                categories['IDE缓存']['count'] += 1
            else:
                categories['其他目录']['dirs'].append(dir_path)
                categories['其他目录']['size'] += dir_size
                categories['其他目录']['count'] += 1

        # 移除空分类
        return {k: v for k, v in categories.items() if v['count'] > 0}
    
    def preview_plugin_cleanup(self):
        """预览将要清理的内容，优化为一屏显示"""
        plugin_name = self.plugin_path.name
        terminal_height = get_terminal_height()
        
        # 预留空间：标题(3行) + 分隔线(2行) + 总结(4行) + 确认提示(2行) = 11行
        max_content_lines = terminal_height - 11
        
        # 标题
        print(color_text(f"\n插件清理预览: {plugin_name}", Colors.HEADER))
        print(color_text("=" * 50, Colors.HEADER))
        
        # 获取文件类型统计
        file_type_stats = self._get_file_type_stats()
        
        # 获取目录分类
        dir_categories = self._categorize_directories()
        
        # 获取目录结构
        dir_structure = self._get_directory_structure()
        
        # 计算各部分占用行数
        dir_lines = len(dir_categories)
        # 每个分类需要2行（标题行+统计行）
        dir_lines *= 2
        
        # 计算目录树行数（最多显示5个顶级目录，每个最多3级深度）
        tree_lines = 0
        if dir_structure:
            # 限制顶级目录数量
            top_dirs = sorted(dir_structure.items(), key=lambda x: x[1]['_size'], reverse=True)[:5]
            for name, data in top_dirs:
                tree_lines += 1  # 顶级目录
                # 估算子目录行数
                tree_lines += min(len(data['_dirs']) * 2, 6)  # 每个顶级目录最多显示3个子目录
        
        type_lines = len(file_type_stats)
        
        # 如果目录和文件类型总行数超过限制，则减少显示
        if dir_lines + tree_lines + type_lines + 7 > max_content_lines:  # +7 为固定标题和分隔线
            # 按比例分配空间
            total_available = max_content_lines - 7
            # 确保目录分类至少显示，即使空间有限
            dir_ratio = max(0.2, min(0.3, dir_lines / (dir_lines + tree_lines + type_lines)))
            tree_ratio = min(0.4, tree_lines / (dir_lines + tree_lines + type_lines))
            
            max_dir_lines = int(total_available * dir_ratio)
            max_tree_lines = int(total_available * tree_ratio)
            max_type_lines = total_available - max_dir_lines - max_tree_lines
            
            # 限制目录显示
            if dir_lines > max_dir_lines:
                # 只显示最大的几个分类
                sorted_categories = sorted(dir_categories.items(), key=lambda x: x[1]['size'], reverse=True)
                max_categories = max(1, max_dir_lines // 2)  # 确保至少显示1个分类
                dir_categories = dict(sorted_categories[:max_categories])
            
            # 限制文件类型显示
            if type_lines > max_type_lines:
                file_type_stats = file_type_stats[:max_type_lines]
        
        # 显示目录分类
        if self.dirs_to_delete and dir_categories:
            print(color_text(f"\n将删除目录分类:", Colors.OKBLUE))
            for category, data in dir_categories.items():
                print(f"  [目录] {category}: {data['count']} 个目录, {color_text(format_size(data['size']), Colors.WARNING)}")
        
        # 显示目录树结构
        if dir_structure:
            print(color_text(f"\n主要目录结构:", Colors.OKBLUE))
            # 限制顶级目录数量
            top_dirs = sorted(dir_structure.items(), key=lambda x: x[1]['_size'], reverse=True)[:5]
            
            for i, (name, data) in enumerate(top_dirs):
                is_last = i == len(top_dirs) - 1
                connector = '└─' if is_last else '├─'
                print(f"  {connector} {name}/ ({format_size(data['_size'])})")
                
                # 显示子目录（最多3个）
                if data['_dirs']:
                    sorted_subdirs = sorted(data['_dirs'].items(), key=lambda x: x[1]['_size'], reverse=True)[:3]
                    for j, (subname, subdata) in enumerate(sorted_subdirs):
                        is_last_sub = j == len(sorted_subdirs) - 1
                        sub_connector = '    └─' if is_last and is_last_sub else '    ├─'
                        print(f"  {sub_connector} {subname}/ ({format_size(subdata['_size'])})")
                
                # 如果有更多子目录，显示省略号
                if len(data['_dirs']) > 3:
                    print(f"        ... 还有 {len(data['_dirs'])-3} 个子目录")
        
        # 显示文件类型统计
        if self.files_to_delete:
            print(color_text(f"\n将删除的文件类型统计:", Colors.OKBLUE))
            for ext, stats in file_type_stats:
                ext_name = ext if ext else "(无扩展名)"
                print(f"  [文件] {ext_name} - {stats['count']} 个文件, {color_text(format_size(stats['size']), Colors.WARNING)}")

            # 显示最大的几个文件
            print(color_text(f"\n最大的文件 (显示前5个):", Colors.OKBLUE))
            display_count = min(5, len(self.files_to_delete))
            for file_path, file_size in self.files_to_delete[:display_count]:
                try:
                    rel_path = file_path.relative_to(self.plugin_path)
                    rel_path_str = str(rel_path)
                except ValueError:
                    # 如果无法计算相对路径，直接使用文件名
                    rel_path_str = file_path.name

                # 截断过长的路径
                if len(rel_path_str) > 50:
                    parts = rel_path_str.split(os.sep)
                    if len(parts) > 2:
                        rel_path_str = ".../" + os.sep.join(parts[-2:])

                print(f"  [文件] {rel_path_str} - {color_text(format_size(file_size), Colors.WARNING)}")
        
        # 显示总计信息
        print(color_text("\n" + "-" * 50, Colors.OKCYAN))
        total_dirs = len(self.dirs_to_delete)
        total_files = len(self.files_to_delete)
        print(f"累计将删除: {total_dirs} 个目录, {total_files} 个文件")
        print(f"预计释放空间: {color_text(format_size(self.total_size), Colors.OKGREEN)}")
        print(color_text("-" * 50, Colors.OKCYAN))
        
        return True
    
    def cleanup_plugin(self):
        """执行清理操作"""
        if not self.files_to_delete and not self.dirs_to_delete:
            print("没有找到需要清理的文件。")
            return True
        
        # 删除文件
        for file_path, _ in self.files_to_delete:
            try:
                if self.verbose:
                    print(f"删除文件: {file_path}")
                file_path.unlink()
            except OSError as e:
                print(f"删除文件失败: {file_path} - {e}")
        
        # 删除目录
        for dir_path, _ in self.dirs_to_delete:
            try:
                if self.verbose:
                    print(f"删除目录: {dir_path}")
                shutil.rmtree(dir_path)
            except OSError as e:
                print(f"删除目录失败: {dir_path} - {e}")
        
        print(f"\n清理完成! 释放空间: {format_size(self.total_size)}")
        return True
    
    def generate_report(self, output_file):
        """生成清理报告"""
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(f"UE插件清理报告\n")
            f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"插件路径: {self.plugin_path}\n")
            f.write(f"总计删除: {len(self.dirs_to_delete)} 个目录, {len(self.files_to_delete)} 个文件\n")
            f.write(f"释放空间: {format_size(self.total_size)}\n\n")

            if self.dirs_to_delete:
                f.write("删除的目录:\n")
                for dir_path, dir_size in self.dirs_to_delete:
                    try:
                        rel_path = dir_path.relative_to(self.plugin_path)
                    except ValueError:
                        rel_path = dir_path
                    f.write(f"  {rel_path}/ - {format_size(dir_size)}\n")

            if self.files_to_delete:
                f.write("\n删除的文件:\n")
                for file_path, file_size in self.files_to_delete:
                    try:
                        rel_path = file_path.relative_to(self.plugin_path)
                    except ValueError:
                        rel_path = file_path
                    f.write(f"  {rel_path} - {format_size(file_size)}\n")

def process_single_path(plugin_path, args, index=None, total=None, need_confirm=True):
    """处理单个路径（文件或目录）"""
    print(f"\n{'='*60}")

    if total and total > 1:
        print(f"[第 {index}/{total}] 正在处理: {plugin_path.name}")

    # 创建清理器实例
    cleaner = PluginCleaner(plugin_path, args.verbose)

    # 扫描插件（如果是zip文件，process_zip_file会处理整个流程）
    if not cleaner.scan_plugin(need_confirm=need_confirm):
        return False, f"扫描失败: {plugin_path}"

    # 检查是否是zip文件（如果scan_plugin返回True，说明已处理完zip）
    if cleaner.is_zip_file():
        return True, f"压缩包已处理: {plugin_path.name}"

    # 显示预览
    cleaner.preview_plugin_cleanup()

    # 确认删除（批量模式下不需要再次确认）
    if need_confirm:
        print(color_text("\n[警告] 此操作将永久删除上述文件和目录!", Colors.WARNING))
        confirm = input("确认删除? (y/N): ").strip().lower()

        if confirm != 'y':
            return False, f"操作已取消: {plugin_path}"

    # 执行清理
    if cleaner.cleanup_plugin():
        if args.report:
            cleaner.generate_report(args.report)
            print(f"清理报告已保存到: {args.report}")
        return True, f"清理成功: {plugin_path}"
    else:
        return False, f"清理失败: {plugin_path}"

def cleanup_old_temp_dirs():
    """清理脚本同目录下遗留的临时目录"""
    script_dir = Path(__file__).parent
    temp_prefix = "temp_"

    try:
        cleaned = 0
        for item in script_dir.iterdir():
            if item.is_dir() and item.name.startswith(temp_prefix):
                try:
                    shutil.rmtree(item)
                    print(f"清理遗留临时目录: {item.name}")
                    cleaned += 1
                except Exception as e:
                    print(f"清理失败: {item.name} - {e}")

        if cleaned > 0:
            print(f"已清理 {cleaned} 个遗留临时目录\n")
    except Exception as e:
        print(f"检查临时目录时出错: {e}\n")

def main():
    # 启动时清理遗留的临时目录
    cleanup_old_temp_dirs()

    parser = argparse.ArgumentParser(
        description='UE插件智能清理工具 - 清理PDB等冗余文件',
        epilog='支持多个文件/目录: file1.zip file2.zip directory1 directory2'
    )
    parser.add_argument('paths', nargs='*', default=['.'], help='插件路径或压缩包路径（可指定多个）')
    parser.add_argument('-v', '--verbose', action='store_true', help='详细输出')
    parser.add_argument('--report', metavar='FILE', help='保存清理报告到指定文件')
    parser.add_argument('--batch', action='store_true', help='批量模式（自动确认所有操作）')
    parser.add_argument('--summary', action='store_true', help='显示处理摘要')

    args = parser.parse_args()

    # 解析所有路径
    paths = []
    for path_str in args.paths:
        p = Path(path_str).resolve()
        if p.exists():
            paths.append(p)
        else:
            print(f"警告: 路径不存在: {p}")

    if not paths:
        print("错误: 没有找到有效的路径")
        sys.exit(1)

    # 处理多个路径
    results = []
    start_time = time.time()

    if len(paths) == 1:
        # 单个文件处理
        success, message = process_single_path(paths[0], args, need_confirm=True)
        results.append((success, message, paths[0].name))
    else:
        # 多个文件处理
        total = len(paths)
        print(f"\n批量处理模式: 共 {total} 个项目")
        print(f"临时目录将使用脚本同目录: {Path(__file__).parent}\n")

        # 统计所有项目的信息
        all_dirs = 0
        all_files = 0
        all_size = 0
        print("正在扫描所有项目...")

        for i, path in enumerate(paths, 1):
            print(f"  [{i}/{total}] {path.name}")
            cleaner = PluginCleaner(path, False)  # 静默模式扫描

            # 只扫描，不处理
            if not cleaner.is_zip_file():
                # 对于普通目录，扫描并统计
                if cleaner._scan_only():
                    all_dirs += len(cleaner.dirs_to_delete)
                    all_files += len(cleaner.files_to_delete)
                    all_size += cleaner.total_size
            else:
                # 对于zip文件，模拟扫描
                all_dirs += 1  # 预估至少有一个Source目录
                all_files += 1
                all_size += 1024  # 预估1KB

        print(f"\n扫描完成！")
        print(f"累计将删除: {all_dirs} 个目录, {all_files} 个文件")
        print()

        # 一次性确认
        if not args.batch:
            confirm = input("是否继续? (Y/n): ").strip().lower()
            if confirm == 'n':
                print("操作已取消。")
                sys.exit(0)

        print(f"\n开始批量处理...")
        for i, path in enumerate(paths, 1):
            # 批量模式下处理：不显示预览和确认，直接清理
            print(f"\n{'='*60}")
            print(f"[第 {i}/{total}] 正在处理: {path.name}")

            cleaner = PluginCleaner(path, args.verbose)

            # 直接处理（跳过预览和确认）
            if not cleaner.scan_plugin(need_confirm=False):
                results.append((False, f"扫描失败: {path}", path.name))
                print(f"\n[{i}/{total}] 扫描失败: {path.name}")
                continue

            # 检查是否是zip文件
            if cleaner.is_zip_file():
                results.append((True, f"压缩包已处理: {path.name}", path.name))
                print(f"\n[{i}/{total}] 压缩包已处理: {path.name}")
                continue

            # 执行清理
            if cleaner.cleanup_plugin():
                if args.report:
                    cleaner.generate_report(args.report)
                    print(f"清理报告已保存到: {args.report}")
                results.append((True, f"清理成功: {path.name}", path.name))
                print(f"\n[{i}/{total}] 清理成功: {path.name}")
            else:
                results.append((False, f"清理失败: {path.name}", path.name))
                print(f"\n[{i}/{total}] 清理失败: {path.name}")

            time.sleep(0.2)  # 短暂延迟让用户看清

    # 显示摘要
    elapsed = time.time() - start_time
    print(f"\n{'='*60}")
    print("处理摘要:")
    print(f"  总用时: {elapsed:.2f} 秒")

    success_count = sum(1 for s, _, _ in results if s)
    failed_count = len(results) - success_count

    print(f"  成功: {success_count}/{len(results)}")
    if failed_count > 0:
        print(f"  失败: {failed_count}/{len(results)}")

    if args.summary or len(paths) > 1:
        print(f"\n详细结果:")
        for success, message, name in results:
            status = "[成功]" if success else "[失败]"
            print(f"  {status} {name}")

    if failed_count > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
