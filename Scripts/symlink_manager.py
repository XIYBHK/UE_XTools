
import os
import subprocess
import ctypes
import sys

def is_admin():
    """检查当前用户是否拥有管理员权限"""
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def create_symlink():
    """引导用户创建目录软链接"""
    print("\n--- 创建软链接 ---")
    source_path = input("请输入【源文件夹】的完整路径 (即您想要引用的文件夹):\n> ")
    link_path = input("请输入【软链接】的完整路径 (即您想在何处创建链接文件夹):\n> ")

    # 校验路径
    if not os.path.isdir(source_path):
        print(f"\n错误：源文件夹路径不存在或不是一个文件夹: {source_path}")
        return

    if os.path.exists(link_path):
        print(f"\n错误：目标链接路径已经存在文件或文件夹: {link_path}")
        return
    
    # 确保目标链接的父目录存在
    parent_dir = os.path.dirname(link_path)
    if not os.path.exists(parent_dir):
        print(f"\n错误：目标链接的父目录不存在: {parent_dir}")
        return

    print(f"\n即将执行: 从 '{source_path}' 创建链接到 '{link_path}'")
    try:
        # 使用 cmd.exe 的 mklink 命令，/D 表示创建目录链接
        # 使用 subprocess.run 来执行命令，并捕获输出
        result = subprocess.run(
            f'mklink /D "{link_path}" "{source_path}"',
            shell=True,
            check=True,
            capture_output=True,
            text=True,
            encoding='gbk' # Windows cmd 默认使用 gbk 编码
        )
        print("\n成功！软链接已创建。")
        print(f"输出: {result.stdout.strip()}")
    except subprocess.CalledProcessError as e:
        print("\n错误：创建软链接失败。")
        print(f"命令执行出错: {e}")
        print(f"错误输出: {e.stderr.strip()}")
    except Exception as e:
        print(f"\n发生未知错误: {e}")

def remove_symlink():
    """引导用户移除目录软链接"""
    print("\n--- 移除软链接 ---")
    link_path = input("请输入要【移除的软链接】的完整路径:\n> ")

    # 校验路径
    if not os.path.exists(link_path):
        print(f"\n错误：路径不存在: {link_path}")
        return
        
    # 在Windows上，目录软链接被 os.path.isdir() 识别为目录，
    # 但 os.path.islink() 也能正确识别 mklink 创建的软链接。
    # 最简单的删除方法是直接使用 os.rmdir()，它能安全地删除软链接本身而不影响源目录。
    
    if not os.path.islink(link_path):
        # 如果 os.path.islink 返回 False，但它可能是一个Junction点，也尝试删除
        print(f"\n警告：'{link_path}' 不是一个标准的符号链接。将尝试作为目录进行移除。")
        print("请确保您指定的确实是软链接，而不是一个真实的文件夹。")
        confirm = input("确认要继续吗？(y/n): ")
        if confirm.lower() != 'y':
            print("操作已取消。 ולא")
            return

    try:
        os.rmdir(link_path)
        print(f"\n成功！软链接 '{link_path}' 已被移除。")
        print("注意：原始的源文件夹未受任何影响。")
    except OSError as e:
        print(f"\n错误：移除软链接失败: {e}")
        print("请检查路径是否正确，或者该链接是否正在被其他程序使用。")
    except Exception as e:
        print(f"\n发生未知错误: {e}")


def main():
    """主函数，显示菜单并处理用户输入"""
    if not is_admin():
        print("错误：需要管理员权限来创建或移除软链接。 ולא")
        print("请右键点击此脚本，选择“以管理员身份运行”。")
        input("按 Enter 键退出...")
        sys.exit(1)

    while True:
        print("\n==============================")
        print("  Windows 软链接管理脚本")
        print("==============================")
        print("请选择要执行的操作:")
        print("  1. 创建目录软链接")
        print("  2. 移除目录软链接")
        print("  3. 退出脚本")
        
        choice = input("请输入选项 (1, 2, or 3):\n> ")

        if choice == '1':
            create_symlink()
        elif choice == '2':
            remove_symlink()
        elif choice == '3':
            print("脚本已退出。")
            break
        else:
            print("\n无效的选项，请输入 1, 2, 或 3。")
        
        input("\n按 Enter 键继续...")
        # 清屏
        os.system('cls' if os.name == 'nt' else 'clear')


if __name__ == "__main__":
    main()
