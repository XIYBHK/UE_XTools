#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批量替换 BLUEPRINTASSIST_API 为 XTOOLS_BLUEPRINTASSIST_API
"""

import os
import re
from pathlib import Path

def replace_in_file(file_path):
    """替换单个文件中的API宏"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # 替换 BLUEPRINTASSIST_API
        content = content.replace('BLUEPRINTASSIST_API', 'XTOOLS_BLUEPRINTASSIST_API')
        
        # 如果内容有变化，写回文件
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, file_path
        
        return False, None
    except Exception as e:
        print(f"错误处理文件 {file_path}: {e}")
        return False, None

def main():
    # 源代码根目录
    source_root = Path(__file__).parent
    
    # 需要处理的文件扩展名
    extensions = ['.h', '.cpp', '.inl']
    
    modified_files = []
    total_files = 0
    
    print(f"开始扫描目录: {source_root}")
    print(f"查找文件类型: {', '.join(extensions)}")
    print("-" * 60)
    
    # 遍历所有子目录
    for root, dirs, files in os.walk(source_root):
        for file in files:
            file_path = Path(root) / file
            
            # 检查文件扩展名
            if file_path.suffix in extensions:
                total_files += 1
                modified, path = replace_in_file(file_path)
                
                if modified:
                    rel_path = file_path.relative_to(source_root)
                    modified_files.append(str(rel_path))
                    print(f"✓ 已修改: {rel_path}")
    
    print("-" * 60)
    print(f"\n处理完成!")
    print(f"扫描文件总数: {total_files}")
    print(f"修改文件数量: {len(modified_files)}")
    
    if modified_files:
        print("\n修改的文件列表:")
        for file in modified_files:
            print(f"  - {file}")
    else:
        print("\n没有文件需要修改")

if __name__ == '__main__':
    main()
