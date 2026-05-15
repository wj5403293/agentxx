import subprocess
import sys

def update_all_langgraph_packages():
    try:
        # 获取所有已安装的包
        result = subprocess.check_output(
            [sys.executable, "-m", "pip", "list", "--format=freeze"],
            text=True
        )
        # 筛选出 langgraph/langchain 相关包
        langgraph_packages = [
            line.split("==")[0] for line in result.splitlines()
            if line.startswith("langgraph") or line.startswith("langchain")
        ]
        
        if not langgraph_packages:
            print("未找到 langgraph/langchain 相关包")
            return
        
        # 批量更新
        print(f"即将更新以下 langgraph/langchain 包：{langgraph_packages}")
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "--upgrade"] + langgraph_packages,
            check=True
        )
        print("所有 langgraph/langchain 包更新完成！")
        
    except subprocess.CalledProcessError as e:
        print(f"更新失败：{e}")
    except Exception as e:
        print(f"执行出错：{e}")

if __name__ == "__main__":
    update_all_langgraph_packages()