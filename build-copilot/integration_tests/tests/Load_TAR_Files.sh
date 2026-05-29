#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/tabular_dvh_plan_uncertainty.tgz \
  "${TEST_FILES_ROOT}"/tabular_dvh_purely_absolute.tgz \
  "${TEST_FILES_ROOT}"/tabular_dvh_purely_relative.tgz \
  \
  -o TestConditions \
    -p Conditions='line_sample_count(37)'


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/meshing_phantom_dalek.obj.tgz \
  "${TEST_FILES_ROOT}"/torus.obj.tgz \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(2)'


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_rectangular_anisovoxelvol_DCMA_logo.tgz \
  \
  -o TestConditions \
    -p Conditions='image_array_count(1)'

