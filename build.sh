#!/bin/bash
set -e

# Apollo Local Build Script
# This script builds Apollo locally for development/testing
# Supports both native builds and Docker-based builds

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
USE_DOCKER="${USE_DOCKER:-auto}"  # auto, yes, no

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

function print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

function print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

function print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

function check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is not installed. Please install it first."
        return 1
    fi
    return 0
}

function check_docker() {
    if command -v docker &> /dev/null && docker info &> /dev/null; then
        return 0
    fi
    return 1
}

function check_dependencies() {
    print_info "Checking dependencies..."
    
    local missing_deps=()
    
    if ! check_command cmake; then
        missing_deps+=("cmake")
    fi
    
    if ! check_command ninja; then
        missing_deps+=("ninja")
    fi
    
    if ! check_command node; then
        missing_deps+=("node")
    fi
    
    if ! check_command npm; then
        missing_deps+=("npm")
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        if [ "$USE_DOCKER" == "auto" ] && check_docker; then
            print_warn "Missing dependencies: ${missing_deps[*]}"
            print_info "Docker detected. Will use Docker for build instead."
            return 2  # Special return code for Docker fallback
        else
            print_error "Missing dependencies: ${missing_deps[*]}"
            print_info "Install instructions:"
            
            if [[ "$OSTYPE" == "darwin"* ]]; then
                print_info "  macOS (Homebrew):"
                print_info "    brew install cmake ninja node"
            elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
                print_info "  Linux (Debian/Ubuntu):"
                print_info "    sudo apt-get install cmake ninja-build nodejs npm"
                print_info "  Linux (Fedora):"
                print_info "    sudo dnf install cmake ninja-build nodejs npm"
            fi
            
            if check_docker; then
                print_info ""
                print_info "Alternatively, use Docker:"
                print_info "  USE_DOCKER=yes $0"
            fi
            exit 1
        fi
    fi
    
    # Check CMake version
    local cmake_version=$(cmake --version | head -n1 | cut -d' ' -f3)
    local required_version="3.25.0"
    if [ "$(printf '%s\n' "$required_version" "$cmake_version" | sort -V | head -n1)" != "$required_version" ]; then
        if [ "$USE_DOCKER" == "auto" ] && check_docker; then
            print_warn "CMake version $cmake_version is too old. Required: $required_version or higher"
            print_info "Docker detected. Will use Docker for build instead."
            return 2  # Special return code for Docker fallback
        else
            print_error "CMake version $cmake_version is too old. Required: $required_version or higher"
            if check_docker; then
                print_info "Use Docker instead: USE_DOCKER=yes $0"
            fi
            exit 1
        fi
    fi
    
    print_info "All dependencies found ✓"
    return 0
}

function configure_cmake() {
    print_info "Configuring CMake..."
    
    cd "$SCRIPT_DIR"
    
    local cmake_args=(
        "-B" "$BUILD_DIR"
        "-G" "Ninja"
        "-S" "."
        "-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )
    
    # Enable tests for development
    if [ "${BUILD_TESTS:-OFF}" == "ON" ]; then
        cmake_args+=("-DBUILD_TESTS=ON")
    fi
    
    # Enable warnings as errors for stricter checking
    if [ "${BUILD_WERROR:-OFF}" == "ON" ]; then
        cmake_args+=("-DBUILD_WERROR=ON")
    fi
    
    # macOS specific options
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS uses system frameworks, no special flags needed
        print_info "Detected macOS build"
    fi
    
    # Linux specific options
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        print_info "Detected Linux build"
        # Enable common Linux features
        cmake_args+=(
            "-DSUNSHINE_ENABLE_WAYLAND=ON"
            "-DSUNSHINE_ENABLE_X11=ON"
            "-DSUNSHINE_ENABLE_DRM=ON"
        )
        
        # CUDA is optional on Linux
        if command -v nvcc &> /dev/null; then
            print_info "CUDA detected, enabling CUDA support"
            cmake_args+=("-DSUNSHINE_ENABLE_CUDA=ON")
        else
            print_warn "CUDA not found, building without CUDA support"
            cmake_args+=("-DSUNSHINE_ENABLE_CUDA=OFF")
        fi
    fi
    
    print_info "Running: cmake ${cmake_args[*]}"
    cmake "${cmake_args[@]}"
    
    print_info "CMake configuration complete ✓"
}

function build() {
    print_info "Building Apollo..."
    
    cd "$SCRIPT_DIR"
    
    # Get number of CPU cores for parallel build
    local num_cores
    if [[ "$OSTYPE" == "darwin"* ]]; then
        num_cores=$(sysctl -n hw.ncpu)
    else
        num_cores=$(nproc)
    fi
    
    print_info "Building with $num_cores parallel jobs..."
    
    if ninja -C "$BUILD_DIR" -j "$num_cores"; then
        print_info "Build successful ✓"
        print_info "Binary location: $BUILD_DIR/apollo (or sunshine depending on build)"
        return 0
    else
        print_error "Build failed!"
        return 1
    fi
}

function run_tests() {
    if [ "${BUILD_TESTS:-OFF}" != "ON" ]; then
        return 0
    fi
    
    print_info "Running tests..."
    cd "$SCRIPT_DIR"
    
    if [ -f "$BUILD_DIR/tests/apollo_tests" ] || [ -f "$BUILD_DIR/tests/sunshine_tests" ]; then
        # Find the test executable
        local test_exe
        if [ -f "$BUILD_DIR/tests/apollo_tests" ]; then
            test_exe="$BUILD_DIR/tests/apollo_tests"
        else
            test_exe="$BUILD_DIR/tests/sunshine_tests"
        fi
        
        "$test_exe"
        print_info "Tests completed ✓"
    else
        print_warn "Test executable not found. Build tests first with BUILD_TESTS=ON"
    fi
}

function clean() {
    print_info "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_info "Build directory cleaned ✓"
    else
        print_info "Build directory does not exist, nothing to clean"
    fi
}

function build_with_docker() {
    local command="${1:-build}"
    print_info "Building Apollo using Docker..."
    
    if ! check_docker; then
        print_error "Docker is not available. Please install Docker or use native build."
        exit 1
    fi
    
    # Check if docker-compose is available
    local compose_cmd="docker-compose"
    if ! command -v docker-compose &> /dev/null; then
        # Try docker compose (newer syntax)
        if docker compose version &> /dev/null; then
            compose_cmd="docker compose"
        else
            print_error "Neither 'docker-compose' nor 'docker compose' is available."
            exit 1
        fi
    fi
    
    cd "$SCRIPT_DIR"
    
    # Export environment variables for docker-compose
    export CMAKE_BUILD_TYPE
    export BUILD_TESTS="${BUILD_TESTS:-OFF}"
    export BUILD_WERROR="${BUILD_WERROR:-OFF}"
    
    # Build the Docker image if needed
    print_info "Building/updating Docker image (this may take a few minutes the first time)..."
    # Build the image (ignore errors about existing tags - the image will still be built)
    $compose_cmd -f docker/docker-compose.dev.yml build apollo-build || {
        print_warn "Build had warnings, but continuing..."
        # Check if image exists
        if docker images apollo-dev:latest --format "{{.Repository}}:{{.Tag}}" | grep -q "apollo-dev:latest"; then
            print_info "Image exists, proceeding with build..."
        else
            print_error "Image build failed"
            exit 1
        fi
    }
    
    # Run the build
    print_info "Running build in Docker container..."
    
    # Check if submodules are initialized on host
    if [ ! -f "third-party/moonlight-common-c/enet/CMakeLists.txt" ]; then
        print_warn "Submodules not initialized. Initializing now..."
        git submodule update --init --recursive || {
            print_error "Failed to initialize submodules. Please run: git submodule update --init --recursive"
            exit 1
        }
    fi
    
    # Run the build
    case "$command" in
        build)
            $compose_cmd -f docker/docker-compose.dev.yml run --rm apollo-build \
                bash -c "USE_DOCKER=no ./build.sh build"
            ;;
        configure)
            $compose_cmd -f docker/docker-compose.dev.yml run --rm apollo-build \
                bash -c "USE_DOCKER=no ./build.sh configure"
            ;;
        test)
            BUILD_TESTS=ON $compose_cmd -f docker/docker-compose.dev.yml run --rm apollo-build \
                bash -c "USE_DOCKER=no BUILD_TESTS=ON ./build.sh test"
            ;;
        *)
            print_error "Unknown command for Docker build: $command"
            exit 1
            ;;
    esac
    
    # Fix permissions on build artifacts (files created in container may have wrong ownership)
    print_info "Fixing file permissions..."
    local uid=$(id -u)
    local gid=$(id -g)
    if [ -d "$BUILD_DIR" ]; then
        # Use sudo if needed, or just try without it
        chown -R "$uid:$gid" "$BUILD_DIR" 2>/dev/null || {
            print_warn "Could not fix permissions automatically. You may need to run:"
            print_warn "  sudo chown -R $uid:$gid $BUILD_DIR"
        }
    fi
    
    print_info "Docker build completed ✓"
    print_info "Build artifacts are in: $BUILD_DIR"
}

function show_usage() {
    cat << EOF
Apollo Local Build Script

Usage: $0 [command] [options]

Commands:
  build       Build Apollo (default)
  configure   Only run CMake configuration
  clean       Clean build directory
  test        Build and run tests
  help        Show this help message

Options (environment variables):
  CMAKE_BUILD_TYPE    Build type: Debug, Release, RelWithDebInfo (default: Release)
  BUILD_TESTS         Enable tests: ON or OFF (default: OFF)
  BUILD_WERROR        Enable warnings as errors: ON or OFF (default: OFF)
  USE_DOCKER          Use Docker: auto, yes, no (default: auto)
                      - auto: Use Docker if dependencies are missing
                      - yes:  Always use Docker
                      - no:   Never use Docker (fail if deps missing)

Examples:
  $0                    # Build in Release mode (native or Docker)
  $0 build              # Same as above
  $0 configure          # Only configure, don't build
  $0 clean              # Clean build directory
  BUILD_TESTS=ON $0     # Build with tests enabled
  CMAKE_BUILD_TYPE=Debug BUILD_WERROR=ON $0  # Debug build with strict warnings
  USE_DOCKER=yes $0     # Force Docker build
  USE_DOCKER=no $0      # Force native build (fail if deps missing)

Docker Build:
  The script can automatically use Docker if dependencies are missing.
  Docker image includes all build dependencies, so you don't need to install
  anything locally except Docker itself.

EOF
}

# Main execution
main() {
    local command="${1:-build}"
    
    # Handle Docker mode
    if [ "$USE_DOCKER" == "yes" ]; then
        build_with_docker "$command"
        return $?
    fi
    
    case "$command" in
        build)
            check_dependencies
            local deps_result=$?
            if [ $deps_result -eq 2 ]; then
                # Dependencies missing, fall back to Docker
                build_with_docker build
            else
                configure_cmake
                if build; then
                    print_info "Build completed successfully!"
                    exit 0
                else
                    print_error "Build failed. Check errors above."
                    exit 1
                fi
            fi
            ;;
        configure)
            check_dependencies
            local deps_result=$?
            if [ $deps_result -eq 2 ]; then
                build_with_docker configure
            else
                configure_cmake
            fi
            ;;
        clean)
            clean
            ;;
        test)
            export BUILD_TESTS=ON
            check_dependencies
            local deps_result=$?
            if [ $deps_result -eq 2 ]; then
                build_with_docker test
            else
                configure_cmake
                if build; then
                    run_tests
                else
                    print_error "Build failed, cannot run tests"
                    exit 1
                fi
            fi
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown command: $command"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"

