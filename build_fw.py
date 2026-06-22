#!/usr/bin/env python3
"""Build M5AtomS3 Voice Stick firmware using ESP-IDF directly."""
import os
import sys
import subprocess

# Remove MSYSTEM from environment - this is the key fix for ESP-IDF on Windows
env = os.environ.copy()
env.pop('MSYSTEM', None)
env.pop('MSYSCON', None)

# Set IDF paths
IDF_PATH = r'D:\esp-idf'
IDF_TOOLS_PATH = r'D:\espressif'
env['IDF_PATH'] = IDF_PATH
env['IDF_TOOLS_PATH'] = IDF_TOOLS_PATH

# ESP-IDF Python environment
python_env_dir = r'C:\Users\Administrator\.espressif\python_env\idf5.5_py3.10_env\Scripts'
env['IDF_PYTHON_ENV_PATH'] = os.path.dirname(python_env_dir)  # parent dir

# Find cmake, ninja, and toolchain
tools_bin = [
    python_env_dir,
    os.path.join(IDF_TOOLS_PATH, 'tools', 'cmake', '3.30.2', 'bin'),
    os.path.join(IDF_TOOLS_PATH, 'tools', 'ninja', '1.12.1'),
    os.path.join(IDF_TOOLS_PATH, 'tools', 'xtensa-esp-elf', 'esp-14.2.0_20260121', 'xtensa-esp-elf', 'bin'),
    os.path.join(IDF_TOOLS_PATH, 'tools', 'esp32ulp-elf', '2.38_20240113', 'esp32ulp-elf', 'bin'),
    os.path.join(IDF_TOOLS_PATH, 'tools', 'idf-exe', '1.0.3'),
    os.path.join(IDF_PATH, 'tools'),
    os.path.join(IDF_PATH, 'tools', 'esp_app_trace'),
]

# Add to PATH - check which dirs actually exist
existing_paths = []
for p in tools_bin:
    if os.path.isdir(p):
        existing_paths.append(p)
    else:
        # Try to find the correct version directory
        print(f"Warning: {p} not found")

env['PATH'] = ';'.join(existing_paths) + ';' + env.get('PATH', '')

# Use IDF Python environment's interpreter
idf_python = os.path.join(python_env_dir, 'python.exe')

firmware_dir = r'D:\WAN\yuyinzhushou\firmware'
os.chdir(firmware_dir)

# Clean previous build artifacts
build_dir = os.path.join(firmware_dir, 'build')
if os.path.isdir(build_dir):
    print(f"Removing old build directory: {build_dir}")
    # Use system command for Windows robustness
    subprocess.run(['cmd', '/c', 'rmdir', '/s', '/q', build_dir], capture_output=True)

log_file = r'D:\WAN\yuyinzhushou\build_log.txt'

def run_step(step_name, args, timeout=300):
    with open(log_file, 'a') as log:
        log.write(f'\n=== {step_name} ===\n')
        log.flush()
        result = subprocess.run(
            args, cwd=firmware_dir, env=env,
            capture_output=True, text=True, timeout=timeout
        )
        log.write(f'STDOUT:\n{result.stdout}\n')
        log.write(f'STDERR:\n{result.stderr}\n')
        log.write(f'EXIT CODE: {result.returncode}\n')
        log.flush()
        return result

with open(log_file, 'w') as log:
    log.write(f'Starting build\nCWD: {firmware_dir}\n')
    log.write(f'MSYSTEM in clean env: {env.get("MSYSTEM", "NOT_SET")}\n')
    log.write(f'PATH: {";".join(existing_paths)}\n\n')
    log.flush()

# Step 1: set-target (only needed first time)
result = run_step('set-target esp32s3',
    [idf_python, os.path.join(IDF_PATH, 'tools', 'idf.py'), 'set-target', 'esp32s3'],
    timeout=120
)
if result.returncode != 0:
    print("SET-TARGET FAILED, see build_log.txt")
    sys.exit(1)

# Step 2: build
result = run_step('build',
    [idf_python, os.path.join(IDF_PATH, 'tools', 'idf.py'), 'build'],
    timeout=300
)

if result.returncode == 0:
    print("BUILD SUCCESS!")
else:
    print("BUILD FAILED, see build_log.txt")
    sys.exit(1)
