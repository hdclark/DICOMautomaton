#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/mesh_cube_with_normals_binary.ply \
  "${TEST_FILES_ROOT}"/mesh_cube_with_normals.ply \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(2)'


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/pcloud_cube_with_normals.ply \
  "${TEST_FILES_ROOT}"/pcloud_cube_with_normals_binary.ply \
  \
  -o TestConditions \
    -p Conditions='point_cloud_count(2)'

