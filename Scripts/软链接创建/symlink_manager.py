import os
import subprocess
import ctypes
import sys
from pathlib import Path
from typing import Tuple, Optional

def is_admin() -> bool:
    """检查当前用户是否拥有管理员权限"""
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except Exception:
        return False

def run_as_admin() -> bool:
    """以管理员权限重新启动脚本"""
    try:
        # 获取当前脚本的绝对路径
        if getattr(sys, 'frozen', False):
            # 如果是打包的exe
            script = sys.executable
            params = ' '.join(sys.argv[1:])
        else:
            # 如果是Python脚本
            script = os.path.abspath(sys.argv[0])
            # 确保参数用引号包裹，避免空格问题
            params = f'"{script}"'
            if len(sys.argv) > 1:
                params += ' ' + ' '.join(f'"{arg}"' for arg in sys.argv[1:])
        
        # 使用 ShellExecuteW 请求管理员权限
        ret = ctypes.windll.shell32.ShellExecuteW(
            None,           # hwnd
            "runas",        # 操作：以管理员身份运行
            sys.executable, # 程序：Python解释器
            params,         # 参数：脚本路径及参数
            None,           # 工作目录
            1               # 显示窗口：SW_SHOWNORMAL
        )
        
        # ShellExecuteW 返回值 > 32 表示成功
        if ret > 32:
            sys.exit(0)  # 退出当前进程，让管理员进程接管
        else:
            print(f"提升权限失败，错误代码: {ret}")
            return False
    except Exception as e:
        print(f"提升权限时发生错误: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True

def normalize_path(path_str: str, resolve: bool = True) -> Path:
    """
    规范化路径，去除首尾空格和引号
    
    Args:
        path_str: 路径字符串
        resolve: 是否解析为绝对路径（对于软链接检查应设为False）
    """
    path_str = path_str.strip().strip('"').strip("'")
    path = Path(path_str)
    return path.resolve() if resolve else path.absolute()

def validate_source_path(source_path: Path) -> Tuple[bool, Optional[str]]:
    """验证源路径是否有效"""
    if not source_path.exists():
        return False, f"源文件夹路径不存在: {source_path}"
    if not source_path.is_dir():
        return False, f"源路径不是一个文件夹: {source_path}"
    return True, None

def validate_link_path(link_path: Path) -> Tuple[bool, Optional[str]]:
    """验证链接路径是否有效"""
    if link_path.exists():
        return False, f"目标链接路径已存在: {link_path}"
    
    parent_dir = link_path.parent
    if not parent_dir.exists():
        return False, f"目标链接的父目录不存在: {parent_dir}"
    
    return True, None

def execute_mklink(link_path: Path, source_path: Path) -> Tuple[bool, str]:
    """执行 mklink 命令创建软链接"""
    try:
        result = subprocess.run(
            f'mklink /D "{link_path}" "{source_path}"',
            shell=True,
            check=True,
            capture_output=True,
            text=True,
            encoding='gbk'
        )
        return True, result.stdout.strip()
    except subprocess.CalledProcessError as e:
        return False, f"命令执行失败: {e.stderr.strip()}"
    except Exception as e:
        return False, f"未知错误: {str(e)}"

def create_symlink():
    """引导用户创建目录软链接"""
    print("\n--- 创建软链接 ---")
    print("提示：如果路径包含空格，可以使用引号包裹，也可以不使用")
    print("示例：C:\\Source\\Folder 或 \"C:\\Source\\Folder\"")
    
    source_input = input("\n请输入【源文件夹】的完整路径 (即您想要引用的文件夹):\n> ")
    source_path = normalize_path(source_input)
    
    # 验证源路径
    is_valid, error_msg = validate_source_path(source_path)
    if not is_valid:
        print(f"\n错误：{error_msg}")
        return
    
    print(f"\n源文件夹已确认: {source_path}")
    print(f"源文件夹名称: {source_path.name}")
    
    # 获取目标路径
    print("\n请输入【软链接位置】:")
    print("  方式1: 输入父目录路径，将自动使用源文件夹的名称")
    print(f"         例如: E:\\Target\\Path  (将创建 E:\\Target\\Path\\{source_path.name})")
    print("  方式2: 输入完整的链接路径")
    print(f"         例如: E:\\Target\\Path\\MyLink")
    
    link_input = input("> ")
    link_path = normalize_path(link_input)
    
    # 智能判断：如果目标路径已存在且是目录，则自动添加源文件夹名称
    if link_path.exists() and link_path.is_dir():
        link_path = link_path / source_path.name
        print(f"\n检测到目标是已存在的目录，自动创建子链接:")
        print(f"  最终链接路径: {link_path}")
    
    # 验证链接路径
    is_valid, error_msg = validate_link_path(link_path)
    if not is_valid:
        print(f"\n错误：{error_msg}")
        return
    
    # 显示即将执行的操作
    print(f"\n即将执行:")
    print(f"  源文件夹: {source_path}")
    print(f"  创建链接: {link_path}")
    
    if not confirm_operation("确认创建？"):
        print("操作已取消。")
        return
    
    # 执行创建
    success, message = execute_mklink(link_path, source_path)
    if success:
        print("\n成功！软链接已创建。")
        print(f"输出: {message}")
        print(f"\n现在您可以通过以下路径访问源文件夹:")
        print(f"  {link_path}")
    else:
        print(f"\n错误：创建软链接失败。")
        print(f"详情: {message}")

def confirm_operation(prompt: str) -> bool:
    """统一的确认操作函数"""
    confirm = input(f"{prompt} (y/n): ").strip().lower()
    return confirm == 'y'

def remove_symlink():
    """引导用户移除目录软链接"""
    print("\n--- 移除软链接 ---")
    print("提示：请输入软链接本身的路径，而不是它指向的源文件夹路径")
    
    link_input = input("\n请输入要【移除的软链接】的完整路径:\n> ")
    # 重要：不要resolve，保留软链接本身的路径
    link_path = normalize_path(link_input, resolve=False)
    
    # 校验路径是否存在
    if not link_path.exists():
        print(f"\n错误：路径不存在: {link_path}")
        return
    
    # 显示路径信息
    print(f"\n链接路径: {link_path}")
    if link_path.is_symlink():
        real_path = link_path.resolve()
        print(f"指向目标: {real_path}")
        print(f"链接类型: 符号链接 (Symbolic Link)")
    else:
        print(f"警告：这不是一个标准的符号链接")
        print(f"可能是真实文件夹或Junction点")
    
    # 确认操作
    if not confirm_operation("\n确认要移除此链接吗？"):
        print("操作已取消。")
        return
    
    # 执行删除
    try:
        if link_path.is_symlink() or link_path.is_junction():
            # 对于符号链接和Junction点，使用rmdir
            link_path.rmdir()
            print(f"\n成功！软链接已被移除: {link_path}")
            print("注意：原始的源文件夹未受任何影响。")
        else:
            print(f"\n错误：这不是软链接或Junction点，拒绝删除以防误操作。")
            print(f"如果确实需要删除，请手动操作。")
    except OSError as e:
        print(f"\n错误：移除软链接失败: {e}")
        print("可能的原因：")
        print("  1. 该链接正在被其他程序使用")
        print("  2. 没有足够的权限")
        print("  3. 这不是一个软链接")
    except Exception as e:
        print(f"\n发生未知错误: {e}")

def clear_screen():
    """跨平台清屏"""
    os.system('cls' if os.name == 'nt' else 'clear')

def print_menu():
    """打印主菜单"""
    print("\n==============================")
    print("  Windows 软链接管理脚本")
    print("==============================")
    print("请选择要执行的操作:")
    print("  1. 创建目录软链接")
    print("  2. 移除目录软链接")
    print("  3. 退出脚本")

def handle_user_choice(choice: str) -> bool:
    """处理用户选择，返回是否继续运行"""
    if choice == '1':
        create_symlink()
        return True
    elif choice == '2':
        remove_symlink()
        return True
    elif choice == '3':
        print("脚本已退出。")
        return False
    else:
        print("\n无效的选项，请输入 1, 2, 或 3。")
        return True

def main():
    """主函数，显示菜单并处理用户输入"""
    try:
        if not is_admin():
            print("检测到需要管理员权限。")
            print("正在尝试自动提升权限...")
            
            if run_as_admin():
                # 成功请求提升权限，当前进程会退出
                return
            else:
                # 提升失败，给用户提示
                print("\n自动提升权限失败。")
                print("请手动右键点击此脚本，选择\"以管理员身份运行\"。")
                input("按 Enter 键退出...")
                sys.exit(1)

        # 已有管理员权限，显示提示
        print("已获取管理员权限。\n")

        while True:
            print_menu()
            choice = input("请输入选项 (1, 2, or 3):\n> ").strip()
            
            should_continue = handle_user_choice(choice)
            if not should_continue:
                break
            
            input("\n按 Enter 键继续...")
            clear_screen()
    
    except KeyboardInterrupt:
        print("\n\n用户中断操作，脚本退出。")
    except Exception as e:
        print(f"\n发生未预期的错误: {e}")
        import traceback
        traceback.print_exc()
        input("\n按 Enter 键退出...")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n用户中断操作。")
    except Exception as e:
        # 最外层异常捕获
        print(f"\n程序启动失败: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # 简单粗暴：无论如何都等待
        try:
            input("\n按 Enter 键退出...")
        except:
            # 如果连input都失败了，用pause
            os.system('pause')
