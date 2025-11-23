#!/bin/bash
set -e

# Simple Docker build script for Apollo
# Alternative to using docker-compose

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="apollo-dev"
CONTAINER_NAME="apollo-build-$$"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

function print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

function print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

function print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check Docker
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed. Please install Docker first."
    exit 1
fi

if ! docker info &> /dev/null; then
    print_error "Docker is not running. Please start Docker first."
    exit 1
fi

# Get user ID and group ID (use different variable names since UID is readonly)
DOCKER_UID=$(id -u)
DOCKER_GID=$(id -g)

print_info "Building Docker image (this may take a few minutes the first time)..."
cd "$PROJECT_ROOT"
docker build -f docker/Dockerfile.dev -t "$IMAGE_NAME" .

print_info "Running build in Docker container..."

# Cleanup function
cleanup() {
    print_info "Cleaning up container..."
    docker rm -f "$CONTAINER_NAME" 2>/dev/null || true
}
trap cleanup EXIT

# Run build
docker run --rm \
    --name "$CONTAINER_NAME" \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    -e CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -e BUILD_TESTS="${BUILD_TESTS:-OFF}" \
    -e BUILD_WERROR="${BUILD_WERROR:-OFF}" \
    -e DOCKER_UID="$DOCKER_UID" \
    -e DOCKER_GID="$DOCKER_GID" \
    "$IMAGE_NAME" \
    bash -c "
        # Fix permissions before and after build
        chown -R \$DOCKER_UID:\$DOCKER_GID /workspace/build 2>/dev/null || true
        # Run build
        USE_DOCKER=no ./build.sh ${@:-build}
        # Fix permissions again after build
        chown -R \$DOCKER_UID:\$DOCKER_GID /workspace/build 2>/dev/null || true
    "

print_info "Build completed! Artifacts are in: $PROJECT_ROOT/build"

