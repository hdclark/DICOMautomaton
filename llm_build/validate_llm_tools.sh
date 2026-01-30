#!/usr/bin/env bash
#
# Simple validation script for LLM build tools
# This ensures the files are present, executable, and have valid syntax
#

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${REPO_ROOT}"
cd llm_build/

echo "Validating LLM build tools..."

# Check files exist
echo "  Checking files exist..."
for file in llm_build.sh llm_conan_build.py LLM_BUILD_README.md; do
    if [ ! -f "$file" ]; then
        echo "ERROR: $file not found"
        exit 1
    fi
    echo "    ✓ $file exists"
done

# Check executability
echo "  Checking executability..."
for file in llm_build.sh llm_conan_build.py; do
    if [ ! -x "$file" ]; then
        echo "ERROR: $file is not executable"
        exit 1
    fi
    echo "    ✓ $file is executable"
done

# Check bash syntax
echo "  Checking bash syntax..."
if bash -n llm_build.sh; then
    echo "    ✓ llm_build.sh has valid syntax"
else
    echo "ERROR: llm_build.sh has syntax errors"
    exit 1
fi

# Check Python syntax
echo "  Checking Python syntax..."
if python3 -m py_compile llm_conan_build.py 2>/dev/null; then
    echo "    ✓ llm_conan_build.py has valid syntax"
    # Clean up bytecode files created during validation
    rm -rf __pycache__
else
    echo "ERROR: llm_conan_build.py has syntax errors"
    exit 1
fi

# Test help output
echo "  Testing help output..."
if ./llm_build.sh --help > /dev/null 2>&1; then
    echo "    ✓ llm_build.sh --help works"
else
    echo "ERROR: llm_build.sh --help failed"
    exit 1
fi

if python3 llm_conan_build.py --help > /dev/null 2>&1; then
    echo "    ✓ llm_conan_build.py --help works"
else
    echo "ERROR: llm_conan_build.py --help failed"
    exit 1
fi

# Check documentation has key sections
echo "  Checking documentation completeness..."
required_sections=(
    "Quick Start"
    "Feature Flags"
    "Troubleshooting"
    "Homebrew"
    "Conan"
)

for section in "${required_sections[@]}"; do
    if ! grep -q "$section" LLM_BUILD_README.md; then
        echo "WARNING: LLM_BUILD_README.md missing section: $section"
    else
        echo "    ✓ LLM_BUILD_README.md has $section section"
    fi
done

# Check gitignore includes LLM artifacts
echo "  Checking .gitignore..."
gitignore_entries=(
    "build_llm/"
    "build_conan/"
    ".linuxbrew/"
    ".conan/"
    "__pycache__/"
    ".venv/"
)

cd "${REPO_ROOT}"
for entry in "${gitignore_entries[@]}"; do
    if ! grep -q "$entry" .gitignore; then
        echo "WARNING: .gitignore missing entry: $entry"
    else
        echo "    ✓ .gitignore includes $entry"
    fi
done

echo ""
echo "All validation checks passed! ✓"
echo ""
echo "Note: Full build testing requires network access and dependencies."
echo "The scripts are syntactically valid and should work when executed in"
echo "an environment with network access and appropriate permissions."
