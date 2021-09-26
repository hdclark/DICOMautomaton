
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM --platform=linux/arm/v7 alpine:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Alpine Linux build base."

WORKDIR /scratch_base
COPY docker/build_bases/alpine /scratch_base

# Ensure something in the image can interpret bash she-bangs.
RUN apk add --no-cache bash

RUN /scratch_base/implementation_alpine.sh

