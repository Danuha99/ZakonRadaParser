import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "src"))
from zakon_downloader.app import run
if __name__ == "__main__":
    sys.exit(run())
