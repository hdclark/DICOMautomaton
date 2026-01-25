# Demons Algorithm Implementation for DICOMautomaton

This document outlines the implementation of the demons algorithm for deformable image registration in DICOMautomaton.

## Overview

The demons algorithm is an intensity-based deformable registration method that iteratively computes a deformation field to align a moving image to a fixed (stationary) image. This implementation includes:

- Standard demons algorithm
- Diffeomorphic demons variant (ensures invertible transformations)
- Optional histogram matching (for images with different intensity distributions)
- Automatic handling of dissimilar image orientations and alignments

## Usage

### Basic Usage

```bash
dicomautomaton_dispatcher \
  -f fixed_image.dcm \
  -f moving_image.dcm \
  -o RegisterImagesDemons:MovingImageSelection='last':FixedImageSelection='first'
```

### Advanced Usage with Parameters

```bash
dicomautomaton_dispatcher \
  -f fixed_image.dcm \
  -f moving_image.dcm \
  -o RegisterImagesDemons:\
MovingImageSelection='last':\
FixedImageSelection='first':\
MaxIterations='200':\
UseDiffeomorphic='true':\
UseHistogramMatching='true':\
DeformationFieldSmoothingSigma='2.0':\
TransformName='demons_deformation'
```

## Parameters

### Image Selection
- **MovingImageSelection** (default: "last"): The image array to be registered
- **FixedImageSelection** (default: "first"): The reference image array

### Algorithm Control
- **MaxIterations** (default: 100): Maximum number of iterations
- **ConvergenceThreshold** (default: 0.001): MSE change threshold for convergence
- **UseDiffeomorphic** (default: false): Enable diffeomorphic demons variant
- **UseHistogramMatching** (default: false): Enable intensity normalization

### Regularization
- **DeformationFieldSmoothingSigma** (default: 1.0): Smoothing for deformation field (mm)
- **UpdateFieldSmoothingSigma** (default: 0.5): Smoothing for update field (mm)

### Stability
- **NormalizationFactor** (default: 1.0): Demons force normalization
- **MaxUpdateMagnitude** (default: 2.0): Maximum update per iteration (mm)

### Histogram Matching
- **HistogramBins** (default: 256): Number of bins for histogram matching
- **HistogramOutlierFraction** (default: 0.01): Fraction of outliers to exclude

### Output
- **TransformName** (default: "demons_registration"): Name for the output transformation
- **Metadata**: Custom metadata as "key@value;key2@value2"
- **Verbosity** (default: 1): Logging level (0-2)

## Example Workflows

### 1. Simple Registration
Register two CT images from the same patient:

```bash
dicomautomaton_dispatcher \
  -f ct_baseline.dcm \
  -f ct_followup.dcm \
  -o RegisterImagesDemons
```

### 2. Multi-Modal Registration with Histogram Matching
Register MRI to CT with intensity normalization:

```bash
dicomautomaton_dispatcher \
  -f ct_image.dcm \
  -f mri_image.dcm \
  -o RegisterImagesDemons:\
UseHistogramMatching='true':\
MaxIterations='200':\
DeformationFieldSmoothingSigma='3.0'
```

### 3. High-Quality Diffeomorphic Registration
For applications requiring invertible transformations:

```bash
dicomautomaton_dispatcher \
  -f reference.dcm \
  -f moving.dcm \
  -o RegisterImagesDemons:\
UseDiffeomorphic='true':\
MaxIterations='300':\
DeformationFieldSmoothingSigma='2.0':\
UpdateFieldSmoothingSigma='1.0':\
Verbosity='2'
```

### 4. Apply Resulting Transformation
After registration, apply the deformation to other objects:

```bash
dicomautomaton_dispatcher \
  -f fixed_image.dcm \
  -f moving_image.dcm \
  -o RegisterImagesDemons:TransformName='my_deformation' \
  -o WarpImages:TransformSelection='my_deformation':ImageSelection='last'
```

## Algorithm Details

### Core Steps

1. **Resampling**: Moving image is resampled onto fixed image grid
2. **Histogram Matching** (optional): Intensity distributions are aligned
3. **Iterative Registration**:
   - Compute image gradients (of fixed image)
   - Compute intensity differences (fixed - warped moving)
   - Calculate demons force: `u = -diff·∇ / (|∇|² + diff²/α)`
   - Smooth update field (for diffeomorphic variant)
   - Add/compose update to deformation field
   - Smooth deformation field for regularization
   - Warp moving image with updated field
   - Check convergence
4. **Output**: Store deformation field as Transform3 object

### Parameters Tuning Guide

**For smooth, robust registration:**
- Increase `DeformationFieldSmoothingSigma` (e.g., 3.0-5.0)
- Use `UseDiffeomorphic='true'`
- Limit `MaxUpdateMagnitude` (e.g., 1.0-2.0)

**For capturing fine details:**
- Decrease `DeformationFieldSmoothingSigma` (e.g., 0.5-1.0)
- Increase `MaxIterations` (e.g., 200-500)
- Increase `NormalizationFactor` to slow convergence

**For different modalities:**
- Use `UseHistogramMatching='true'`
- Adjust `HistogramBins` based on image bit depth
- May need higher `MaxIterations`

**For images with different orientations:**
- The algorithm automatically handles resampling
- No special parameters needed
- May increase computational time

## Performance Considerations

- **Memory**: Deformation field requires 3x the memory of input images
- **Speed**: ~1-5 seconds per iteration for typical medical images
- **Convergence**: Typically 50-200 iterations for good results
- **Smoothing**: More expensive but critical for quality

## Implementation Details

### File Structure
- `src/Alignment_Demons.h/cc` - Core algorithm implementation
- `src/Operations/RegisterImagesDemons.h/cc` - User-facing operation
- Integration with existing `deformation_field` class
- Uses Ygor's `planar_image_collection` throughout

### Key Design Decisions
1. **Automatic resampling**: Handles any image orientation/alignment
2. **Separate smoothing**: Update field (diffeomorphic) vs deformation field (regularization)
3. **Conservative defaults**: Favor stability over speed
4. **Extensive parameters**: Allow fine-tuning for various scenarios

## Limitations

1. **Intensity-based**: Requires sufficient intensity variation and correlation
2. **Local minima**: May not find globally optimal registration
3. **Computational cost**: Can be slow for large 3D volumes
4. **Memory intensive**: Stores full 3D vector field
5. **Initialization**: May benefit from pre-alignment (e.g., rigid registration)

## References

- Thirion J-P (1998). "Image matching as a diffusion process: an analogy with Maxwell's demons". Medical Image Analysis 2(3):243-260.
- Vercauteren T, et al. (2009). "Diffeomorphic demons: Efficient non-parametric image registration". NeuroImage 45(1):S61-S72.

## Support

For issues or questions:
- Check operation documentation: `dicomautomaton_dispatcher -u RegisterImagesDemons`
- Review test cases in `integration_tests/`
- Consult main DICOMautomaton documentation

