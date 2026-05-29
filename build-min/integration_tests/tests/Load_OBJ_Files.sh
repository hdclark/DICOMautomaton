#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  "${TEST_FILES_ROOT}"/icosahedron.obj \
  "${TEST_FILES_ROOT}"/5_nested_spheres.obj \
  "${TEST_FILES_ROOT}"/dodecahedron.obj \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(5)'


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/square.obj \
  "${TEST_FILES_ROOT}"/twisted_rhombus.obj \
  \
  -o TestConditions \
    -p Conditions='point_cloud_count(2)'

