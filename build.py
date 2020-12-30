from subprocess import run
from pathlib import Path

if __name__ == '__main__':
    Path('_build').mkdir(exist_ok=True)
    # Path('_build/CMakeCache.txt').unlink(missing_ok=True)

    run(['cmake', '..'], cwd='_build')
