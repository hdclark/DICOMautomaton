#!/usr/bin/env bash
#
# LLM-Friendly Build Script for DICOMautomaton
# 
# This script provides an automated, cross-platform build method using Homebrew/Linuxbrew
# for dependency management. It's designed to be easily understood and executed by LLMs.
#
# Usage:
#   ./llm_build.sh [options]
#
# Options:
#   --help                Show this help message
#   --install             Install after building (default: build only)
#   --clean               Clean build (remove existing build artifacts)
#   --install-prefix DIR  Installation prefix (default: $HOME/.local)
#   --homebrew-prefix DIR Homebrew installation prefix (default: $HOME/.linuxbrew)
#   --build-dir DIR       Build directory (default: build_llm)
#   --jobs N              Number of parallel jobs (default: auto-detect, max 8)
#
# Environment Variables (Feature Flags):
#   WITH_EIGEN=ON|OFF     Enable/disable Eigen support (default: ON)
#   WITH_CGAL=ON|OFF      Enable/disable CGAL support (default: ON)
#   WITH_NLOPT=ON|OFF     Enable/disable NLopt support (default: ON)
#   WITH_SFML=ON|OFF      Enable/disable SFML support (default: ON)
#   WITH_SDL=ON|OFF       Enable/disable SDL support (default: ON)
#   WITH_WT=ON|OFF        Enable/disable Wt support (default: ON)
#   WITH_GNU_GSL=ON|OFF   Enable/disable GNU GSL support (default: ON)
#   WITH_POSTGRES=ON|OFF  Enable/disable PostgreSQL support (default: ON)
#   WITH_JANSSON=ON|OFF   Enable/disable Jansson support (default: ON)
#   WITH_THRIFT=ON|OFF    Enable/disable Apache Thrift support (default: ON)
#
# Example:
#   # Basic build
#   ./llm_build.sh
#
#   # Build and install to custom location
#   ./llm_build.sh --install --install-prefix $HOME/dicomautomaton
#
#   # Clean build with minimal features
#   WITH_CGAL=OFF WITH_WT=OFF ./llm_build.sh --clean --install
#

set -eu
set -o pipefail

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
INSTALL_PREFIX="${INSTALL_PREFIX:-${HOME}/.local}"
HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-${HOME}/.linuxbrew}"
BUILD_DIR="build_llm"
DO_INSTALL="no"
CLEAN_BUILD="no"
JOBS=""

# Build parallelism defaults
DEFAULT_JOBS=4  # Fallback when CPU detection fails (reasonable minimum for modern systems)
MAX_JOBS=8      # Maximum parallel jobs to avoid OOM on memory-constrained systems

# Feature flags with defaults
WITH_EIGEN="${WITH_EIGEN:-ON}"
WITH_CGAL="${WITH_CGAL:-ON}"
WITH_NLOPT="${WITH_NLOPT:-ON}"
WITH_SFML="${WITH_SFML:-ON}"
WITH_SDL="${WITH_SDL:-ON}"
WITH_WT="${WITH_WT:-ON}"
WITH_GNU_GSL="${WITH_GNU_GSL:-ON}"
WITH_POSTGRES="${WITH_POSTGRES:-ON}"
WITH_JANSSON="${WITH_JANSSON:-ON}"
WITH_THRIFT="${WITH_THRIFT:-ON}"

# Utility functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

show_help() {
    grep '^#' "$0" | grep -v '#!/usr/bin/env' | sed 's/^# \?//'
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            show_help
            ;;
        --install)
            DO_INSTALL="yes"
            shift
            ;;
        --clean)
            CLEAN_BUILD="yes"
            shift
            ;;
        --install-prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --homebrew-prefix)
            HOMEBREW_PREFIX="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            ;;
    esac
done

# Determine the repository root
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "${REPO_ROOT}" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    REPO_ROOT="${SCRIPT_DIR}"
fi

cd "${REPO_ROOT}"
log_info "Repository root: ${REPO_ROOT}"
log_info "Install prefix: ${INSTALL_PREFIX}"
log_info "Homebrew prefix: ${HOMEBREW_PREFIX}"
log_info "Build directory: ${BUILD_DIR}"

# Determine number of parallel jobs
if [ -z "${JOBS}" ]; then
    JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo ${DEFAULT_JOBS})
    JOBS=$((JOBS < MAX_JOBS ? JOBS : MAX_JOBS))
fi
log_info "Using ${JOBS} parallel jobs"

# Feature flag summary
log_info "Feature flags:"
log_info "  WITH_EIGEN=${WITH_EIGEN}"
log_info "  WITH_CGAL=${WITH_CGAL}"
log_info "  WITH_NLOPT=${WITH_NLOPT}"
log_info "  WITH_SFML=${WITH_SFML}"
log_info "  WITH_SDL=${WITH_SDL}"
log_info "  WITH_WT=${WITH_WT}"
log_info "  WITH_GNU_GSL=${WITH_GNU_GSL}"
log_info "  WITH_POSTGRES=${WITH_POSTGRES}"
log_info "  WITH_JANSSON=${WITH_JANSSON}"
log_info "  WITH_THRIFT=${WITH_THRIFT}"

# Install Homebrew/Linuxbrew if not present
if [ ! -d "${HOMEBREW_PREFIX}" ] || [ ! -x "${HOMEBREW_PREFIX}/bin/brew" ]; then
    log_info "Installing Homebrew/Linuxbrew to ${HOMEBREW_PREFIX}..."
    
    # Create the directory
    mkdir -p "${HOMEBREW_PREFIX}"
    
    # Clone Homebrew
    if [ ! -d "${HOMEBREW_PREFIX}/Homebrew" ]; then
        log_info "Cloning Homebrew repository..."
        git clone --depth 1 https://github.com/Homebrew/brew "${HOMEBREW_PREFIX}/Homebrew"
    fi
    
    # Create symlink structure
    mkdir -p "${HOMEBREW_PREFIX}/bin"
    ln -sf "${HOMEBREW_PREFIX}/Homebrew/bin/brew" "${HOMEBREW_PREFIX}/bin/brew"
    
    log_success "Homebrew installed"
else
    log_info "Homebrew already installed at ${HOMEBREW_PREFIX}"
fi

# Set up Homebrew environment
export PATH="${HOMEBREW_PREFIX}/bin:${PATH}"
export HOMEBREW_PREFIX
export HOMEBREW_CELLAR="${HOMEBREW_PREFIX}/Cellar"
export HOMEBREW_REPOSITORY="${HOMEBREW_PREFIX}/Homebrew"

# Verify brew is available
if ! command -v brew &> /dev/null; then
    log_error "Homebrew installation failed or not in PATH"
    exit 1
fi

log_info "Homebrew version: $(brew --version | head -n1)"

# Update Homebrew
log_info "Updating Homebrew..."
brew update || log_warn "Homebrew update had issues, continuing anyway"

# Install core dependencies
log_info "Installing core dependencies..."
brew install cmake gcc boost asio zlib || true

# Install optional dependencies based on feature flags
if [ "${WITH_EIGEN}" = "ON" ]; then
    log_info "Installing Eigen..."
    brew install eigen || true
fi

if [ "${WITH_CGAL}" = "ON" ]; then
    log_info "Installing CGAL..."
    brew install cgal || true
fi

if [ "${WITH_NLOPT}" = "ON" ]; then
    log_info "Installing NLopt..."
    brew install nlopt || true
fi

if [ "${WITH_GNU_GSL}" = "ON" ]; then
    log_info "Installing GSL..."
    brew install gsl || true
fi

if [ "${WITH_SFML}" = "ON" ]; then
    log_info "Installing SFML..."
    brew install sfml || true
fi

if [ "${WITH_SDL}" = "ON" ]; then
    log_info "Installing SDL2 and GLEW..."
    brew install sdl2 glew || true
fi

if [ "${WITH_WT}" = "ON" ]; then
    log_info "Installing Wt..."
    brew install wt || true
fi

if [ "${WITH_POSTGRES}" = "ON" ]; then
    log_info "Installing PostgreSQL client libraries..."
    brew install postgresql libpq libpqxx || true
fi

if [ "${WITH_JANSSON}" = "ON" ]; then
    log_info "Installing Jansson..."
    brew install jansson || true
fi

if [ "${WITH_THRIFT}" = "ON" ]; then
    log_info "Installing Apache Thrift..."
    brew install thrift || true
fi

log_success "Dependencies installed"

# Build Ygor
log_info "Building Ygor..."
YGOR_DIR="${REPO_ROOT}/${BUILD_DIR}/ygor"
if [ "${CLEAN_BUILD}" = "yes" ] || [ ! -d "${YGOR_DIR}" ]; then
    rm -rf "${YGOR_DIR}"
    git clone --depth 1 https://github.com/hdclark/Ygor.git "${YGOR_DIR}"
fi

cd "${YGOR_DIR}"
mkdir -p build
cd build

# Set up CMAKE_PREFIX_PATH to include Homebrew
export CMAKE_PREFIX_PATH="${HOMEBREW_PREFIX}:${CMAKE_PREFIX_PATH:-}"
export PKG_CONFIG_PATH="${HOMEBREW_PREFIX}/lib/pkgconfig:${HOMEBREW_PREFIX}/share/pkgconfig:${PKG_CONFIG_PATH:-}"

cmake \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    ..

make -j "${JOBS}"
make install

log_success "Ygor built and installed"

# Build Explicator
log_info "Building Explicator..."
EXPLICATOR_DIR="${REPO_ROOT}/${BUILD_DIR}/explicator"
if [ "${CLEAN_BUILD}" = "yes" ] || [ ! -d "${EXPLICATOR_DIR}" ]; then
    rm -rf "${EXPLICATOR_DIR}"
    git clone --depth 1 https://github.com/hdclark/Explicator.git "${EXPLICATOR_DIR}"
fi

cd "${EXPLICATOR_DIR}"
mkdir -p build
cd build

cmake \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    ..

make -j "${JOBS}"
make install

log_success "Explicator built and installed"

# Build YgorClustering
log_info "Building YgorClustering..."
YGORCLUSTERING_DIR="${REPO_ROOT}/${BUILD_DIR}/ygorclustering"
if [ "${CLEAN_BUILD}" = "yes" ] || [ ! -d "${YGORCLUSTERING_DIR}" ]; then
    rm -rf "${YGORCLUSTERING_DIR}"
    git clone --depth 1 https://github.com/hdclark/YgorClustering.git "${YGORCLUSTERING_DIR}"
fi

cd "${YGORCLUSTERING_DIR}"
mkdir -p build
cd build

cmake \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    ..

make -j "${JOBS}"
make install

log_success "YgorClustering built and installed"

# Update CMAKE_PREFIX_PATH to include install prefix
export CMAKE_PREFIX_PATH="${INSTALL_PREFIX}:${CMAKE_PREFIX_PATH}"
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Build DICOMautomaton
log_info "Building DICOMautomaton..."
cd "${REPO_ROOT}"

# Clean build if requested
if [ "${CLEAN_BUILD}" = "yes" ]; then
    log_info "Removing existing build directory..."
    rm -rf "${BUILD_DIR}/dicomautomaton"
fi

mkdir -p "${BUILD_DIR}/dicomautomaton"
cd "${BUILD_DIR}/dicomautomaton"

# Get version
DCMA_VERSION="$(${REPO_ROOT}/scripts/extract_dcma_version.sh 2>/dev/null || echo "unknown")"

# Configure with CMake
log_info "Configuring DICOMautomaton with CMake..."
cmake \
    -DDCMA_VERSION="${DCMA_VERSION}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    -DMEMORY_CONSTRAINED_BUILD=OFF \
    -DWITH_EIGEN="${WITH_EIGEN}" \
    -DWITH_CGAL="${WITH_CGAL}" \
    -DWITH_NLOPT="${WITH_NLOPT}" \
    -DWITH_SFML="${WITH_SFML}" \
    -DWITH_SDL="${WITH_SDL}" \
    -DWITH_WT="${WITH_WT}" \
    -DWITH_GNU_GSL="${WITH_GNU_GSL}" \
    -DWITH_POSTGRES="${WITH_POSTGRES}" \
    -DWITH_JANSSON="${WITH_JANSSON}" \
    -DWITH_THRIFT="${WITH_THRIFT}" \
    "${REPO_ROOT}"

# Build
log_info "Building DICOMautomaton..."
make -j "${JOBS}" VERBOSE=1

log_success "DICOMautomaton built successfully"

# Install if requested
if [ "${DO_INSTALL}" = "yes" ]; then
    log_info "Installing DICOMautomaton to ${INSTALL_PREFIX}..."
    make install
    log_success "DICOMautomaton installed to ${INSTALL_PREFIX}"
    log_info "Add ${INSTALL_PREFIX}/bin to your PATH to use dicomautomaton_dispatcher"
else
    log_info "Build complete. Binary is at: ${BUILD_DIR}/dicomautomaton/dicomautomaton_dispatcher"
    log_info "To install, run: cd ${BUILD_DIR}/dicomautomaton && make install"
fi

log_success "Build process completed successfully!"
echo ""
log_info "Summary:"
log_info "  Repository: ${REPO_ROOT}"
log_info "  Build directory: ${REPO_ROOT}/${BUILD_DIR}/dicomautomaton"
log_info "  Install prefix: ${INSTALL_PREFIX}"
if [ "${DO_INSTALL}" = "yes" ]; then
    log_info "  Executable: ${INSTALL_PREFIX}/bin/dicomautomaton_dispatcher"
else
    log_info "  Executable: ${BUILD_DIR}/dicomautomaton/dicomautomaton_dispatcher"
fi
echo ""
