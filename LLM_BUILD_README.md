# LLM-Friendly Build Tools for DICOMautomaton

This directory contains alternative build tools designed to be easily understood and executed by Large Language Models (LLMs) and automated build systems. These tools use cross-platform dependency management systems to simplify building DICOMautomaton on Linux systems.

## Overview

Two build methods are provided:

1. **`llm_build.sh`** - Homebrew/Linuxbrew-based build (Bash)
2. **`llm_conan_build.py`** - Conan-based build (Python)

Both methods:
- Work alongside existing build tools without modifications
- Automatically handle dependency installation
- Support all existing feature flags
- Use separate build directories to avoid conflicts
- Provide clear status messages and error reporting

## Quick Start

### Method 1: Homebrew/Linuxbrew Build

```bash
# Make the script executable
chmod +x llm_build.sh

# Basic build
./llm_build.sh

# Build and install to custom location
./llm_build.sh --install --install-prefix $HOME/dicomautomaton

# Clean build with custom features
WITH_CGAL=OFF WITH_WT=OFF ./llm_build.sh --clean --install
```

### Method 2: Conan Build

```bash
# Make the script executable
chmod +x llm_conan_build.py

# Basic build (requires Python 3)
python3 llm_conan_build.py

# Build and install to custom location
python3 llm_conan_build.py --install --install-prefix $HOME/dicomautomaton

# Clean build with custom features
WITH_CGAL=OFF python3 llm_conan_build.py --clean --install
```

## For LLMs and Automated Builds

The simplest way to build DICOMautomaton from scratch:

```bash
# Clone repository
git clone https://github.com/hdclark/DICOMautomaton.git
cd DICOMautomaton

# Option 1: Use Homebrew/Linuxbrew (Bash)
chmod +x llm_build.sh
./llm_build.sh --install

# Option 2: Use Conan (Python)
chmod +x llm_conan_build.py
python3 llm_conan_build.py --install
```

## Detailed Usage

### llm_build.sh Options

```
Usage: ./llm_build.sh [options]

Options:
  --help                Show help message
  --install             Install after building (default: build only)
  --clean               Clean build (remove existing artifacts)
  --install-prefix DIR  Installation prefix (default: $HOME/.local)
  --homebrew-prefix DIR Homebrew installation prefix (default: $HOME/.linuxbrew)
  --build-dir DIR       Build directory (default: build_llm)
  --jobs N              Number of parallel jobs (default: auto, max 8)
```

**Example:**
```bash
./llm_build.sh \
  --clean \
  --install \
  --install-prefix /opt/dicomautomaton \
  --jobs 4
```

### llm_conan_build.py Options

```
Usage: python3 llm_conan_build.py [options]

Options:
  --help                Show help message
  --install             Install after building (default: build only)
  --clean               Clean build (remove existing artifacts)
  --install-prefix DIR  Installation prefix (default: $HOME/.local)
  --build-dir DIR       Build directory (default: build_conan)
  --jobs N              Number of parallel jobs (default: auto, max 8)
```

**Example:**
```bash
python3 llm_conan_build.py \
  --clean \
  --install \
  --install-prefix /opt/dicomautomaton \
  --jobs 4
```

## Feature Flags

Both scripts support the same feature flags through environment variables. The table below shows defaults for the Homebrew/Linuxbrew method:

| Environment Variable | Default (Homebrew) | Default (Conan) | Description |
|---------------------|--------------------|--------------------|-------------|
| `WITH_EIGEN`        | ON                 | ON                 | Enable Eigen linear algebra library |
| `WITH_CGAL`         | ON                 | ON                 | Enable CGAL computational geometry |
| `WITH_NLOPT`        | ON                 | ON                 | Enable NLopt optimization library |
| `WITH_SFML`         | ON                 | ON                 | Enable SFML for graphics/viewer |
| `WITH_SDL`          | ON                 | ON                 | Enable SDL2 for graphics/viewer |
| `WITH_WT`           | ON                 | OFF*               | Enable Wt web interface |
| `WITH_GNU_GSL`      | ON                 | ON                 | Enable GNU Scientific Library |
| `WITH_POSTGRES`     | ON                 | ON                 | Enable PostgreSQL database support |
| `WITH_JANSSON`      | ON                 | OFF*               | Enable Jansson JSON library |
| `WITH_THRIFT`       | ON                 | ON                 | Enable Apache Thrift RPC |

**Notes:**
- \* `WITH_WT` and `WITH_JANSSON` default to OFF for Conan because these packages are not available in Conan Center. These packages need system installation if required when using Conan.

### Using Feature Flags

**Disable specific features:**
```bash
WITH_CGAL=OFF WITH_WT=OFF ./llm_build.sh --install
```

**Minimal build (core features only):**
```bash
WITH_CGAL=OFF \
WITH_NLOPT=OFF \
WITH_SFML=OFF \
WITH_SDL=OFF \
WITH_WT=OFF \
WITH_POSTGRES=OFF \
WITH_JANSSON=OFF \
WITH_THRIFT=OFF \
./llm_build.sh --install
```

**All features enabled:**
```bash
WITH_EIGEN=ON \
WITH_CGAL=ON \
WITH_NLOPT=ON \
WITH_SFML=ON \
WITH_SDL=ON \
WITH_WT=ON \
WITH_GNU_GSL=ON \
WITH_POSTGRES=ON \
WITH_JANSSON=ON \
WITH_THRIFT=ON \
./llm_build.sh --install
```

## Dependencies

### Core Dependencies (Always Required)

Both methods automatically install:
- CMake (build system)
- GCC/G++ (compiler)
- Boost (C++ libraries)
- Zlib (compression)
- ASIO (async I/O)

### DICOMautomaton-Specific Dependencies

Automatically built and installed by both scripts:
- **Ygor** - Core library (https://github.com/hdclark/Ygor)
- **Explicator** - Lexicon library (https://github.com/hdclark/Explicator)
- **YgorClustering** - Clustering algorithms (https://github.com/hdclark/YgorClustering)

### Optional Dependencies (Feature-Dependent)

Controlled by feature flags:
- **Eigen** - Linear algebra (`WITH_EIGEN`)
- **CGAL** - Computational geometry (`WITH_CGAL`)
- **NLopt** - Optimization (`WITH_NLOPT`)
- **GSL** - GNU Scientific Library (`WITH_GNU_GSL`)
- **SFML** - Graphics/viewer (`WITH_SFML`)
- **SDL2** - Graphics/viewer (`WITH_SDL`)
- **GLEW** - OpenGL extension wrangler (with SDL)
- **Wt** - Web interface (`WITH_WT`)
- **PostgreSQL** - Database support (`WITH_POSTGRES`)
- **libpqxx** - PostgreSQL C++ client (with PostgreSQL)
- **Jansson** - JSON parsing (`WITH_JANSSON`)
- **Apache Thrift** - RPC framework (`WITH_THRIFT`)

## Build Directories

Each method uses a separate build directory to avoid conflicts:

### Homebrew/Linuxbrew Build Structure
```
DICOMautomaton/
├── build_llm/                    # Main build directory
│   ├── ygor/                     # Ygor source and build
│   ├── explicator/               # Explicator source and build
│   ├── ygorclustering/           # YgorClustering source and build
│   └── dicomautomaton/           # DICOMautomaton build
│       └── dicomautomaton_dispatcher  # Executable
├── llm_build.sh                  # This script
└── ...
```

### Conan Build Structure
```
DICOMautomaton/
├── build_conan/                  # Main build directory
│   ├── conanfile.txt             # Generated Conan dependencies
│   ├── conan_toolchain.cmake     # Generated CMake toolchain
│   ├── ygor/                     # Ygor source and build
│   ├── explicator/               # Explicator source and build
│   ├── ygorclustering/           # YgorClustering source and build
│   └── dicomautomaton/           # DICOMautomaton build
│       └── dicomautomaton_dispatcher  # Executable
├── llm_conan_build.py            # This script
└── ...
```

## Installation

### Installation Locations

Default installation prefix: `$HOME/.local`

Directory structure after installation:
```
<install_prefix>/
├── bin/
│   └── dicomautomaton_dispatcher
├── lib/
│   ├── libygor.so
│   ├── libexplicator.so
│   └── libygorclustering.so
├── include/
│   ├── YgorMath.h
│   ├── Explicator/
│   └── YgorClustering/
└── share/
    └── DICOMautomaton/
```

### Using the Installed Binary

Add the installation prefix to your PATH:
```bash
export PATH="$HOME/.local/bin:$PATH"

# Test the installation
dicomautomaton_dispatcher --version
dicomautomaton_dispatcher -u  # Show available operations
```

## Comparison: Homebrew vs Conan

### Homebrew/Linuxbrew (`llm_build.sh`)

**Pros:**
- Simple Bash script
- Works on macOS and Linux
- No Python required
- Larger package ecosystem
- Easier to debug

**Cons:**
- Larger installation (full Homebrew)
- Less reproducible (rolling releases)
- Slower initial setup

**Best for:**
- Users familiar with Bash
- Systems where Python is unavailable
- macOS builds
- Interactive development

### Conan (`llm_conan_build.py`)

**Pros:**
- Explicit dependency versions
- Better reproducibility
- Smaller footprint
- Native CMake integration
- Python-based (better error handling)

**Cons:**
- Requires Python 3
- Smaller package ecosystem
- Some packages not in Conan Center (Wt, Jansson)
- More complex troubleshooting

**Best for:**
- CI/CD pipelines
- Reproducible builds
- Version-pinned dependencies
- Cross-platform C++ projects

## Troubleshooting

### Common Issues

#### 1. Homebrew Installation Fails

**Problem:** Cannot clone Homebrew repository
```
fatal: unable to access 'https://github.com/Homebrew/brew'
```

**Solution:**
- Check internet connectivity
- Try with a proxy if behind firewall
- Manually clone to `$HOMEBREW_PREFIX/Homebrew`

#### 2. Conan Package Not Found

**Problem:** Package not available in Conan Center
```
ERROR: Package 'wt' not found in remotes
```

**Solution:**
- Set feature flag to OFF: `WITH_WT=OFF python3 llm_conan_build.py`
- Or install package via system package manager first

#### 3. Build Runs Out of Memory

**Problem:** Build process killed due to OOM
```
c++: fatal error: Killed signal terminated program cc1plus
```

**Solution:**
- Reduce parallel jobs: `--jobs 1`
- Use memory-constrained build
- Close other applications
- Add swap space

#### 4. CMake Cannot Find Dependencies

**Problem:** CMake reports missing libraries
```
CMake Error: Could not find package 'Eigen3'
```

**Solution:**
- For Homebrew: Dependencies should be auto-detected
- For Conan: Check that `conan install` completed successfully
- Manually set `CMAKE_PREFIX_PATH` if needed

#### 5. Ygor/Explicator Build Fails

**Problem:** Dependency build fails during compilation

**Solution:**
- Check build logs in `build_*/ygor/build` or similar
- Ensure all core dependencies are installed
- Try clean build: `--clean`
- Update to latest version: delete dependency dir and rebuild

### Getting Help

1. **Check build logs:** Detailed output is in build directories
2. **Try clean build:** Use `--clean` flag to remove cached artifacts
3. **Verify dependencies:** Ensure feature flag dependencies are available
4. **Reduce features:** Disable optional features to isolate issues
5. **Consult main README:** See [README.md](README.md) for more build options

## Advanced Usage

### Custom Homebrew Location

```bash
./llm_build.sh \
  --homebrew-prefix /opt/homebrew \
  --install-prefix /opt/dicomautomaton \
  --install
```

### Using Pre-installed Dependencies

If you already have some dependencies installed, you can:

1. Set `CMAKE_PREFIX_PATH` before running:
```bash
export CMAKE_PREFIX_PATH="/usr/local:/opt/local:$CMAKE_PREFIX_PATH"
./llm_build.sh --install
```

2. For Conan, modify the generated `conanfile.txt` to remove packages

### Building Specific Dependency Version

Edit the clone commands in the scripts to use specific tags:

```bash
# In llm_build.sh or llm_conan_build.py, change:
git clone --depth 1 https://github.com/hdclark/Ygor.git
# to:
git clone --depth 1 --branch v20210101 https://github.com/hdclark/Ygor.git
```

### Integration with CI/CD

#### GitHub Actions Example
```yaml
name: Build with LLM Tools

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with Conan
        run: |
          python3 llm_conan_build.py --install --install-prefix $HOME/dcma
      - name: Test
        run: |
          export PATH="$HOME/dcma/bin:$PATH"
          dicomautomaton_dispatcher --version
```

#### GitLab CI Example
```yaml
build:
  image: ubuntu:22.04
  script:
    - chmod +x llm_conan_build.py
    - python3 llm_conan_build.py --install
    - export PATH="$HOME/.local/bin:$PATH"
    - dicomautomaton_dispatcher --version
```

## Relationship to Existing Build System

These LLM-friendly tools are **complementary** to the existing build system:

### Existing Build Tools (Unchanged)
- `compile_and_install.sh` - Distribution-aware builds (Debian, Arch, etc.)
- `CMakeLists.txt` - Core CMake build configuration
- `PKGBUILD` - Arch Linux package build
- `docker/` - Docker-based builds
- `.gitlab-ci.yml` - CI/CD configuration

### LLM Build Tools (New)
- `llm_build.sh` - Homebrew-based automated build
- `llm_conan_build.py` - Conan-based automated build
- `LLM_BUILD_README.md` - Documentation (this file)

### Key Differences

| Aspect | Existing Tools | LLM Tools |
|--------|---------------|-----------|
| Target Users | Experienced developers | LLMs and automation |
| Dependency Mgmt | System package managers | Homebrew/Conan |
| Requires Root | Often yes | No |
| Distribution-Specific | Yes | No |
| Build Directory | Various | `build_llm/` or `build_conan/` |
| Documentation | README, wiki | This file |

### When to Use Each

**Use existing tools when:**
- Building distribution packages (.deb, .pkg.tar.zst)
- Working on specific distributions (Debian, Arch)
- Contributing to the project
- Using Docker containers

**Use LLM tools when:**
- Automating builds with LLMs/AI
- Building on diverse Linux distributions
- No root access available
- Need reproducible builds across systems
- CI/CD with consistent dependencies

## Performance Considerations

### Build Times

Approximate build times on a modern 8-core system:

| Component | Time |
|-----------|------|
| Homebrew setup | 2-5 min |
| Dependency installation | 5-15 min |
| Ygor build | 1-2 min |
| Explicator build | <1 min |
| YgorClustering build | <1 min |
| DICOMautomaton build | 10-30 min |
| **Total** | **20-55 min** |

### Disk Space

Approximate disk space requirements:

| Component | Size |
|-----------|------|
| Homebrew installation | 500 MB - 2 GB |
| Conan cache | 1-3 GB |
| Dependencies (source + build) | 2-5 GB |
| DICOMautomaton build | 2-4 GB |
| **Total** | **5-15 GB** |

### Memory Requirements

Recommended RAM for parallel builds:

- 1 job: 2 GB
- 2 jobs: 4 GB
- 4 jobs: 8 GB
- 8 jobs: 16 GB

If build fails due to memory:
```bash
./llm_build.sh --jobs 1  # Use single thread
```

## Contributing

These build tools are designed to be simple and maintainable. When contributing:

1. Keep scripts self-contained
2. Maintain clear status messages
3. Handle errors gracefully
4. Document all options
5. Test on multiple distributions
6. Preserve compatibility with existing builds

## License

These build scripts are provided under the same license as DICOMautomaton (GPLv3).
See [LICENSE.txt](LICENSE.txt) for details.

## Support

For issues with these build tools:

1. Check this documentation
2. Review troubleshooting section
3. Try clean build with `--clean`
4. Open issue on GitHub with:
   - Which script used (Homebrew or Conan)
   - Full command line used
   - Feature flags set
   - Build log output
   - System information (OS, version)

For general DICOMautomaton issues, see the main [README.md](README.md).
