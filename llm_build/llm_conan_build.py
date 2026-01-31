#!/usr/bin/env python3
"""
LLM-Friendly Conan Build Script for DICOMautomaton

This script provides an automated, reproducible build method using Conan
for C++ dependency management. It's designed to be easily understood and
executed by LLMs.

Prerequisites:
    - Python 3 with venv module (python3-venv on Debian/Ubuntu)
    - Git
    - CMake
    - C++ compiler (gcc/g++ or clang)
    - pkg-config
    - curl (for Conan package downloads)

    On Debian/Ubuntu:
        apt-get install -y python3 python3-venv git cmake gcc g++ pkg-config curl

Usage:
    python3 llm_conan_build.py [options]

Options:
    --help                Show this help message
    --install             Install after building (default: build only)
    --clean               Clean build (remove existing build artifacts)
    --install-prefix DIR  Installation prefix (default: $HOME/.local)
    --build-dir DIR       Build directory (default: build_conan)
    --jobs N              Number of parallel jobs (default: auto-detect, max 8)

Environment Variables (Feature Flags):
    WITH_EIGEN=ON|OFF     Enable/disable Eigen support (default: ON)
    WITH_CGAL=ON|OFF      Enable/disable CGAL support (default: OFF)
    WITH_NLOPT=ON|OFF     Enable/disable NLopt support (default: OFF)
    WITH_SFML=ON|OFF      Enable/disable SFML support (default: OFF)
    WITH_SDL=ON|OFF       Enable/disable SDL support (default: OFF)
    WITH_WT=ON|OFF        Enable/disable Wt support (default: OFF, not in Conan)
    WITH_GNU_GSL=ON|OFF   Enable/disable GNU GSL support (default: ON)
    WITH_POSTGRES=ON|OFF  Enable/disable PostgreSQL support (default: OFF)
    WITH_JANSSON=ON|OFF   Enable/disable Jansson support (default: OFF, not in Conan)
    WITH_THRIFT=ON|OFF    Enable/disable Apache Thrift support (default: OFF)

Example:
    # Basic build
    python3 llm_conan_build.py

    # Build and install to custom location
    python3 llm_conan_build.py --install --install-prefix $HOME/dicomautomaton

    # Clean build with minimal features
    WITH_CGAL=OFF python3 llm_conan_build.py --clean --install
"""

import argparse
import os
import sys
import subprocess
import shutil
import shlex
from pathlib import Path
import multiprocessing

# Build parallelism constants
DEFAULT_JOBS = 4  # Fallback when CPU detection fails (reasonable minimum for modern systems)
MAX_JOBS = 8      # Maximum parallel jobs to avoid OOM on memory-constrained systems

# Safe environment variables to extract from Conan build scripts
# These are compiler, linker, and path variables that are safe to accept
SAFE_BUILD_ENV_VARS = {
    'PATH', 'LD_LIBRARY_PATH', 'PKG_CONFIG_PATH',
    'CPATH', 'LIBRARY_PATH', 
    'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
    'AR', 'AS', 'RANLIB', 'STRIP'
}

# ANSI color codes for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

def log_info(msg):
    print(f"{Colors.BLUE}[INFO]{Colors.NC} {msg}")

def log_success(msg):
    print(f"{Colors.GREEN}[SUCCESS]{Colors.NC} {msg}")

def log_warn(msg):
    print(f"{Colors.YELLOW}[WARN]{Colors.NC} {msg}")

def log_error(msg):
    print(f"{Colors.RED}[ERROR]{Colors.NC} {msg}", file=sys.stderr)

def run_command(cmd, cwd=None, env=None, check=True):
    """Run a shell command and handle errors."""
    log_info(f"Running: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            check=check,
            capture_output=False,
            text=True
        )
        return result
    except subprocess.CalledProcessError as e:
        log_error(f"Command failed with exit code {e.returncode}")
        if check:
            sys.exit(1)
        return None

def check_system_dependencies():
    """Check for required system dependencies and provide helpful error messages."""
    required_commands = {
        'bash': 'Bash shell',
        'git': 'Git version control system',
        'cmake': 'CMake build system',
        'gcc': 'GNU C compiler (or alternative C compiler)',
        'g++': 'GNU C++ compiler (or alternative C++ compiler)',
        'pkg-config': 'pkg-config tool',
        'curl': 'curl download tool',
    }
    
    missing = []
    for cmd, description in required_commands.items():
        if not shutil.which(cmd):
            missing.append(f"  - {cmd}: {description}")
    
    if missing:
        log_error("Missing required system dependencies:")
        for item in missing:
            print(item)
        print()
        log_error("On Debian/Ubuntu, install with:")
        print("  apt-get install -y python3 python3-venv git cmake gcc g++ pkg-config curl")
        print()
        log_error("On other distributions, install equivalent packages for your system.")
        return False
    
    log_success("All required system dependencies found")
    return True

def setup_virtual_environment(build_root):
    """Create and set up a Python virtual environment for Conan."""
    venv_dir = build_root / '.venv'
    
    log_info(f"Setting up Python virtual environment at {venv_dir}")
    
    # Create virtual environment if it doesn't exist
    if not venv_dir.exists():
        try:
            import venv
            log_info("Creating virtual environment...")
            venv.create(venv_dir, with_pip=True, clear=False)
            log_success("Virtual environment created")
        except Exception as e:
            log_error(f"Failed to create virtual environment: {e}")
            return None, None
    else:
        log_info("Virtual environment already exists")
    
    # Determine the Python executable path in the venv
    if os.name == 'nt':  # Windows
        venv_python = venv_dir / 'Scripts' / 'python.exe'
        venv_pip = venv_dir / 'Scripts' / 'pip.exe'
    else:  # Unix-like (Linux, macOS)
        venv_python = venv_dir / 'bin' / 'python'
        venv_pip = venv_dir / 'bin' / 'pip'
    
    if not venv_python.exists():
        log_error(f"Virtual environment Python not found at {venv_python}")
        return None, None
    
    log_success(f"Using virtual environment Python: {venv_python}")
    return venv_python, venv_pip

def ensure_conan_installed(venv_pip, venv_dir):
    """Ensure Conan is installed in the virtual environment."""
    # Determine the conan executable path in the venv
    if os.name == 'nt':  # Windows
        venv_conan = venv_dir / 'Scripts' / 'conan.exe'
    else:  # Unix-like (Linux, macOS)
        venv_conan = venv_dir / 'bin' / 'conan'
    
    # Check if conan is available in the venv
    try:
        if venv_conan.exists():
            # Get conan version
            version_result = subprocess.run(
                [str(venv_conan), '--version'],
                capture_output=True,
                text=True,
                check=False
            )
            if version_result.returncode == 0:
                log_info(f"Conan already installed: {version_result.stdout.strip()}")
                return venv_conan
    except Exception:
        pass
    
    log_info("Conan not found in virtual environment, installing via pip...")
    try:
        # Install conan in the virtual environment
        result = subprocess.run(
            [str(venv_pip), 'install', 'conan'],
            capture_output=False,
            text=True,
            check=True
        )
        log_success("Conan installed successfully in virtual environment")
        
        # Verify installation
        if not venv_conan.exists():
            log_error(f"Conan executable not found at {venv_conan} after installation")
            return None
        
        return venv_conan
    except subprocess.CalledProcessError as e:
        log_error(f"Failed to install Conan: {e}")
        return None
    except Exception as e:
        log_error(f"Failed to install Conan: {e}")
        return None

def get_repo_root():
    """Get the repository root directory."""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--show-toplevel'],
            capture_output=True,
            text=True,
            check=True
        )
        return Path(result.stdout.strip())
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Git not available or not in a git repository, use script location
        return Path(__file__).parent.parent.absolute()

def create_conanfile(build_dir, features):
    """Create conanfile.txt with required dependencies."""
    conanfile_content = "[requires]\n"
    
    # Core dependencies
    conanfile_content += "boost/1.83.0\n"
    conanfile_content += "zlib/1.3.1\n"
    
    # Optional dependencies based on features
    if features.get('eigen', True):
        conanfile_content += "eigen/3.4.0\n"
    
    if features.get('cgal', True):
        # CGAL has complex dependencies, may need special handling
        conanfile_content += "cgal/5.6\n"
    
    if features.get('nlopt', True):
        conanfile_content += "nlopt/2.7.1\n"
    
    if features.get('gsl', True):
        conanfile_content += "gsl/2.7.1\n"
    
    if features.get('sfml', True):
        conanfile_content += "sfml/2.6.1\n"
    
    if features.get('sdl', True):
        conanfile_content += "sdl/2.28.5\n"
        conanfile_content += "glew/2.2.0\n"
    
    if features.get('postgres', True):
        conanfile_content += "libpq/15.4\n"
        conanfile_content += "libpqxx/7.7.5\n"
    
    if features.get('thrift', True):
        conanfile_content += "thrift/0.19.0\n"
    
    # Generators for CMake integration
    conanfile_content += "\n[generators]\n"
    conanfile_content += "CMakeDeps\n"
    conanfile_content += "CMakeToolchain\n"
    conanfile_content += "VirtualBuildEnv\n"  # Provides environment activation scripts with CC, CXX, etc.
    
    # Write the file
    conanfile_path = build_dir / "conanfile.txt"
    with open(conanfile_path, 'w') as f:
        f.write(conanfile_content)
    
    log_info(f"Created conanfile.txt at {conanfile_path}")
    return conanfile_path

def build_dependency(name, repo_url, install_prefix, build_root, cmake_prefix_path, jobs, conan_build_env):
    """Build a dependency from source (Ygor, Explicator, YgorClustering)."""
    log_info(f"Building {name}...")
    
    dep_dir = build_root / name.lower()
    if not dep_dir.exists():
        log_info(f"Cloning {name}...")
        run_command(['git', 'clone', '--depth', '1', repo_url, str(dep_dir)])
    
    build_dir = dep_dir / 'build'
    build_dir.mkdir(exist_ok=True)
    
    # Set up environment with Conan paths and install_prefix paths
    cmake_env = update_build_environment(conan_build_env, install_prefix)
    cmake_env['CMAKE_PREFIX_PATH'] = cmake_prefix_path
    
    # Debug: Print environment variables to verify they're set
    log_info(f"Compiler environment for {name}:")
    for var in ['CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS']:
        value = cmake_env.get(var, '(not set)')
        log_info(f"  {var}={value}")
    
    # Configure
    cmake_args = [
        'cmake',
        f'-DCMAKE_INSTALL_PREFIX={install_prefix}',
        '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_PREFIX_PATH={cmake_prefix_path}',
        str(dep_dir)
    ]
    run_command(cmake_args, cwd=build_dir, env=cmake_env)
    
    # Build
    run_command(['make', '-j', str(jobs)], cwd=build_dir, env=cmake_env)
    
    # Install
    run_command(['make', 'install'], cwd=build_dir, env=cmake_env)
    
    log_success(f"{name} built and installed")

def get_conan_build_environment(build_root):
    """
    Extract build environment from Conan installation.
    Conan 2.x generates environment files that we need to source.
    Uses VirtualBuildEnv generator output to get CC, CXX, CFLAGS, etc.
    """
    env = os.environ.copy()
    
    # Conan 2.x with VirtualBuildEnv generates conanbuildenv-*.sh scripts
    # Look for these scripts to extract build environment
    scripts_to_try = [
        build_root / 'conanbuildenv-release.sh',  # Release build
        build_root / 'conanbuildenv.sh',          # Generic
        build_root / 'conanbuild.sh',             # Legacy/alternative name
    ]
    
    script_found = False
    for conan_build_script in scripts_to_try:
        if conan_build_script.exists():
            script_found = True
            log_info(f"Extracting Conan build environment from {conan_build_script.name}...")
            
            # Parse the conan build script to extract environment variables
            try:
                # Run a shell command that sources the script and prints the environment
                # Use shlex.quote to prevent shell injection
                script_path = shlex.quote(str(conan_build_script))
                result = subprocess.run(
                    ['bash', '-c', f'source {script_path} && env'],
                    capture_output=True,
                    text=True,
                    check=False
                )
                
                if result.returncode == 0:
                    # Parse environment variables from output
                    # Only accept variables from our whitelist to avoid injection issues
                    vars_found = []
                    for line in result.stdout.split('\n'):
                        if '=' not in line:
                            continue
                        
                        key, _, value = line.partition('=')
                        # Only update if the key is in our whitelist
                        # The whitelist itself ensures safety
                        if key in SAFE_BUILD_ENV_VARS:
                            env[key] = value
                            vars_found.append(key)
                    
                    if vars_found:
                        log_success(f"Conan build environment extracted: {', '.join(sorted(vars_found))}")
                    else:
                        log_warn("No target environment variables found in Conan script")
                    break  # Stop after first successful script
                else:
                    log_warn(f"Could not source {conan_build_script.name}, trying next...")
            except Exception as e:
                log_warn(f"Failed to extract Conan environment from {conan_build_script.name}: {e}")
    
    if not script_found:
        log_warn("No Conan build environment scripts found")
        log_warn("Expected scripts: conanbuildenv-release.sh, conanbuildenv.sh, or conanbuild.sh")
        log_warn("Build may fail if Conan dependencies are not found")
        log_info("Setting default compiler environment variables...")
        
        # Set default compilers if not already set
        # Prefer gcc/g++ as they're more common, but fall back to clang if gcc not available
        if 'CC' not in env and 'CXX' not in env:
            if shutil.which('gcc') and shutil.which('g++'):
                env['CC'] = 'gcc'
                env['CXX'] = 'g++'
                log_info("Using gcc/g++ as default compilers")
            elif shutil.which('clang') and shutil.which('clang++'):
                env['CC'] = 'clang'
                env['CXX'] = 'clang++'
                log_info("Using clang/clang++ as default compilers")
            else:
                log_warn("No suitable C/C++ compiler found (gcc/g++ or clang/clang++)")
        elif 'CC' not in env:
            # CXX is set but CC is not - try to match the compiler family
            cxx_basename = os.path.basename(env.get('CXX', ''))
            if 'g++' in cxx_basename and shutil.which('gcc'):
                env['CC'] = 'gcc'
            elif 'clang++' in cxx_basename and shutil.which('clang'):
                env['CC'] = 'clang'
        elif 'CXX' not in env:
            # CC is set but CXX is not - try to match the compiler family
            cc_basename = os.path.basename(env.get('CC', ''))
            if 'gcc' in cc_basename and shutil.which('g++'):
                env['CXX'] = 'g++'
            elif 'clang' in cc_basename and shutil.which('clang++'):
                env['CXX'] = 'clang++'
    
    return env

def update_build_environment(base_env, install_prefix):
    """
    Update environment with install_prefix paths.
    Returns a copy of the environment with updated PATH, PKG_CONFIG_PATH, and LD_LIBRARY_PATH.
    """
    env = base_env.copy()
    
    lib_path = str(Path(install_prefix) / 'lib')
    bin_path = str(Path(install_prefix) / 'bin')
    pkg_config_path = str(Path(install_prefix) / 'lib' / 'pkgconfig')
    
    # Update PATH to include install_prefix/bin
    if 'PATH' in env:
        env['PATH'] = f"{bin_path}:{env['PATH']}"
    else:
        env['PATH'] = f"{bin_path}:{os.environ.get('PATH', '')}"
    
    # Update PKG_CONFIG_PATH
    if 'PKG_CONFIG_PATH' in env:
        env['PKG_CONFIG_PATH'] = f"{pkg_config_path}:{env['PKG_CONFIG_PATH']}"
    else:
        env['PKG_CONFIG_PATH'] = pkg_config_path
    
    # Update LD_LIBRARY_PATH for runtime linking
    if 'LD_LIBRARY_PATH' in env:
        env['LD_LIBRARY_PATH'] = f"{lib_path}:{env['LD_LIBRARY_PATH']}"
    else:
        env['LD_LIBRARY_PATH'] = lib_path
    
    return env

def main():
    parser = argparse.ArgumentParser(
        description='LLM-friendly Conan build script for DICOMautomaton',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('--install', action='store_true',
                        help='Install after building')
    parser.add_argument('--clean', action='store_true',
                        help='Clean build (remove existing artifacts)')
    parser.add_argument('--install-prefix', default=os.path.expanduser('~/.local'),
                        help='Installation prefix (default: $HOME/.local)')
    parser.add_argument('--build-dir', default='build_conan',
                        help='Build directory (default: build_conan)')
    parser.add_argument('--jobs', type=int,
                        help='Number of parallel jobs (default: auto-detect, max 8)')
    
    args = parser.parse_args()
    
    # Check for required system dependencies
    log_info("Checking system dependencies...")
    if not check_system_dependencies():
        sys.exit(1)
    
    # Determine repository root
    repo_root = get_repo_root()
    log_info(f"Repository root: {repo_root}")
    
    # Expand paths
    install_prefix = Path(args.install_prefix).expanduser().absolute()
    build_root = repo_root / args.build_dir
    
    log_info(f"Install prefix: {install_prefix}")
    log_info(f"Build directory: {build_root}")
    
    # Determine number of jobs
    if args.jobs:
        jobs = args.jobs
    else:
        try:
            jobs = min(multiprocessing.cpu_count(), MAX_JOBS)
        except NotImplementedError:
            jobs = DEFAULT_JOBS
    log_info(f"Using {jobs} parallel jobs")
    
    # Get feature flags from environment
    # Note: CGAL, NLOPT, SFML, SDL, POSTGRES, and THRIFT are disabled by default
    # to simplify the build and reduce download/compile time. Enable as needed.
    features = {
        'eigen': os.getenv('WITH_EIGEN', 'ON') == 'ON',
        'cgal': os.getenv('WITH_CGAL', 'OFF') == 'ON',
        'nlopt': os.getenv('WITH_NLOPT', 'OFF') == 'ON',
        'sfml': os.getenv('WITH_SFML', 'OFF') == 'ON',
        'sdl': os.getenv('WITH_SDL', 'OFF') == 'ON',
        'wt': os.getenv('WITH_WT', 'OFF') == 'ON',  # Not available in Conan
        'gsl': os.getenv('WITH_GNU_GSL', 'ON') == 'ON',
        'postgres': os.getenv('WITH_POSTGRES', 'OFF') == 'ON',
        'jansson': os.getenv('WITH_JANSSON', 'OFF') == 'ON',  # Not in Conan
        'thrift': os.getenv('WITH_THRIFT', 'OFF') == 'ON',
    }
    
    # Display feature flags
    log_info("Feature flags:")
    for feature, enabled in features.items():
        log_info(f"  WITH_{feature.upper()}={'ON' if enabled else 'OFF'}")
    
    # Note about unavailable packages
    if features['wt']:
        log_warn("Wt is not available in Conan Center, will need system installation")
    if features['jansson']:
        log_warn("Jansson is not available in Conan Center, will need system installation")
    
    # Clean build if requested
    if args.clean and build_root.exists():
        log_info(f"Removing existing build directory: {build_root}")
        shutil.rmtree(build_root)
    
    # Create build directory
    build_root.mkdir(exist_ok=True)
    
    # Set up Python virtual environment
    venv_python, venv_pip = setup_virtual_environment(build_root)
    if venv_python is None or venv_pip is None:
        log_error("Failed to set up virtual environment")
        sys.exit(1)
    
    # Ensure Conan is installed in the virtual environment
    venv_conan = ensure_conan_installed(venv_pip, build_root / '.venv')
    if venv_conan is None:
        log_error("Failed to install Conan in virtual environment")
        sys.exit(1)
    
    # Create conanfile.txt
    create_conanfile(build_root, features)
    
    # Ensure Conan profile exists (required for Conan 2.x)
    log_info("Setting up Conan profile...")
    profile_detect_cmd = [str(venv_conan), 'profile', 'detect', '--force']
    run_command(profile_detect_cmd, cwd=build_root, check=False)
    
    # Install Conan dependencies using venv conan executable
    log_info("Installing Conan dependencies...")
    conan_install_cmd = [
        str(venv_conan), 'install', str(build_root),
        '--build=missing',
        '-s', 'compiler.cppstd=17',
        '-s', 'build_type=Release'
    ]
    run_command(conan_install_cmd, cwd=build_root)
    log_success("Conan dependencies installed")
    
    # Extract Conan build environment
    conan_build_env = get_conan_build_environment(build_root)
    
    # Set up CMAKE_PREFIX_PATH
    conan_toolchain = build_root / 'conan_toolchain.cmake'
    cmake_prefix_paths = [
        str(install_prefix),
        str(build_root)
    ]
    cmake_prefix_path = ':'.join(cmake_prefix_paths)
    
    # Build Ygor
    build_dependency(
        'Ygor',
        'https://github.com/hdclark/Ygor.git',
        install_prefix,
        build_root,
        cmake_prefix_path,
        jobs,
        conan_build_env
    )
    
    # Build Explicator
    build_dependency(
        'Explicator',
        'https://github.com/hdclark/Explicator.git',
        install_prefix,
        build_root,
        cmake_prefix_path,
        jobs,
        conan_build_env
    )
    
    # Build YgorClustering
    build_dependency(
        'YgorClustering',
        'https://github.com/hdclark/YgorClustering.git',
        install_prefix,
        build_root,
        cmake_prefix_path,
        jobs,
        conan_build_env
    )
    
    # Build DICOMautomaton
    log_info("Building DICOMautomaton...")
    
    dcma_build_dir = build_root / 'dicomautomaton'
    dcma_build_dir.mkdir(exist_ok=True)
    
    # Get version
    try:
        version_script = repo_root / 'scripts' / 'extract_dcma_version.sh'
        result = subprocess.run(
            [str(version_script)],
            capture_output=True,
            text=True,
            cwd=repo_root,
            check=False  # Don't raise on non-zero exit; check stdout instead
        )
        dcma_version = result.stdout.strip() or 'unknown'
    except (FileNotFoundError, OSError) as e:
        # Script not found or permission denied
        log_warn(f"Could not extract version ({type(e).__name__}): {e}")
        dcma_version = 'unknown'
    
    # Prepare CMake arguments with Conan environment
    cmake_env = update_build_environment(conan_build_env, install_prefix)
    cmake_env['CMAKE_PREFIX_PATH'] = cmake_prefix_path
    
    cmake_args = [
        'cmake',
        f'-DDCMA_VERSION={dcma_version}',
        f'-DCMAKE_INSTALL_PREFIX={install_prefix}',
        '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_PREFIX_PATH={cmake_prefix_path}',
    ]
    
    # Add toolchain file if it exists
    if conan_toolchain.exists():
        cmake_args.append(f'-DCMAKE_TOOLCHAIN_FILE={conan_toolchain}')
    
    # Add feature flags
    cmake_args.extend([
        '-DMEMORY_CONSTRAINED_BUILD=OFF',
        f'-DWITH_EIGEN={"ON" if features["eigen"] else "OFF"}',
        f'-DWITH_CGAL={"ON" if features["cgal"] else "OFF"}',
        f'-DWITH_NLOPT={"ON" if features["nlopt"] else "OFF"}',
        f'-DWITH_SFML={"ON" if features["sfml"] else "OFF"}',
        f'-DWITH_SDL={"ON" if features["sdl"] else "OFF"}',
        f'-DWITH_WT={"ON" if features["wt"] else "OFF"}',
        f'-DWITH_GNU_GSL={"ON" if features["gsl"] else "OFF"}',
        f'-DWITH_POSTGRES={"ON" if features["postgres"] else "OFF"}',
        f'-DWITH_JANSSON={"ON" if features["jansson"] else "OFF"}',
        f'-DWITH_THRIFT={"ON" if features["thrift"] else "OFF"}',
    ])
    
    cmake_args.append(str(repo_root))
    
    # Configure
    log_info("Configuring DICOMautomaton with CMake...")
    run_command(cmake_args, cwd=dcma_build_dir, env=cmake_env)
    
    # Build
    log_info("Building DICOMautomaton...")
    run_command(['make', '-j', str(jobs), 'VERBOSE=1'], cwd=dcma_build_dir, env=cmake_env)
    
    log_success("DICOMautomaton built successfully")
    
    # Install if requested
    if args.install:
        log_info(f"Installing DICOMautomaton to {install_prefix}...")
        run_command(['make', 'install'], cwd=dcma_build_dir, env=cmake_env)
        log_success(f"DICOMautomaton installed to {install_prefix}")
        log_info(f"Add {install_prefix}/bin to your PATH to use dicomautomaton_dispatcher")
    else:
        log_info(f"Build complete. Binary is at: {dcma_build_dir}/dicomautomaton_dispatcher")
        log_info(f"To install, run: cd {dcma_build_dir} && make install")
    
    log_success("Build process completed successfully!")
    print()
    log_info("Summary:")
    log_info(f"  Repository: {repo_root}")
    log_info(f"  Build directory: {dcma_build_dir}")
    log_info(f"  Install prefix: {install_prefix}")
    if args.install:
        log_info(f"  Executable: {install_prefix}/bin/dicomautomaton_dispatcher")
    else:
        log_info(f"  Executable: {dcma_build_dir}/dicomautomaton_dispatcher")
    print()

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        log_warn("\nBuild interrupted by user")
        sys.exit(130)
    except Exception as e:
        log_error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
