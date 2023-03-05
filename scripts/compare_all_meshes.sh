
POSITIONAL_ARGS=()
OPERATIONS=""
VERBOSE=0

while [[ $# -gt 0 ]]; do
  case $1 in
    -s|--show)
      OPERATIONS+="-o SDL_Viewer"
      shift # past argument
      ;;
    -v|--verbose)
      VERBOSE=1
      shift # past argument
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done


declare -a shapes=("Sphere(10)" "Sphere(5)" "Sphere(7)")
# declare -a shapes=("Sphere(10)" ")

printf "| %-15s | %-13s | %-13s | %-15s | %-15s | %-10s |\n" Shape "Hausdorff 1" "Hausdorff 2" "Surface Area 1" "Aurface Area 2" "Area Diff"
for shape in "${shapes[@]}"
do
  # echo "Computing differences for $shape"
  OUTPUT="$(dicomautomaton_dispatcher \
    -v \
    -o GenerateMeshes \
      -p resolution="0.25, 0.25, 0.25" \
      -p MeshLabel="original" \
      -p Objects="$shape" \
    -o GenerateSyntheticImages \
      -p NumberOfImages='128' \
      -p NumberOfRows='64', \
      -p NumberOfColumns='64' \
      -p NumberOfChannels='1' \
      -p SliceThickness='1' \
      -p SpacingBetweenSlices='1' \
      -p VoxelWidth='1.0' \
      -p VoxelHeight='1.0' \
      -p ImageAnchor='-14.0, -14.0, -14.0' \
      -p ImagePosition='0.0, 0.0, 0.0' \
      -p ImageOrientationColumn='1.0, 0.0, 0.0' \
      -p ImageOrientationRow='0.0, 1.0, 0.0' \
      -p InstanceNumber='1' \
      -p AcquisitionNumber='1' \
      -p VoxelValue='0.0' \
      -p StipleValue='nan' \
    -o ConvertMeshesToContours \
      -p ROILabel="original contours" \
      -p MeshSelection="last" \
      -p ImageSelection="last" \
    -o ConvertContoursToMeshes \
      -p NormalizedROILabelRegex=".*" \
      -p ROILabelRegex=".*" \
      -p MeshLabel="reconstructed" \
      -p Method="direct" \
    -o CompareMeshes \
      -p MeshSelection1="#-0" \
      -p MeshSelection2="#-1" \
    $OPERATIONS 2>&1)"
  if [ "$VERBOSE" == 1 ]; then
    echo "$OUTPUT"
  fi
  HD_LINE=$(grep "HAUSDORFF DISTANCE" <<< "$OUTPUT")
  SA_LINE=$(grep "SURFACE AREA: First mesh" <<< "$OUTPUT")
  SA_DIFF_LINE=$(grep "SURFACE AREA difference" <<< "$OUTPUT")

  HD1=$(sed -r "s/.*([0-9]+\.[0-9]+) or.*/\1/g" <<< $HD_LINE)
  HD2=$(sed -r "s/.*or ([0-9]+\.[0-9]+).*/\1/g" <<< $HD_LINE)

  SA1=$(sed -r "s/.*First mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $SA_LINE)
  SA2=$(sed -r "s/.*second mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $SA_LINE)

  SA_DIFF=$(sed -r "s/.*difference: (-?[0-9]+\.[0-9]+).*/\1/g" <<< $SA_DIFF_LINE)

  # echo "$HD1"
  # echo "$HD2"
  # echo "$SA1"
  # echo "$SA2"
  # echo "$SA_DIFF"
  printf "| %-15s | %-13.3f | %-13.3f | %-15.3f | %-15.3f | %-10.3f |\n" $shape $HD1 $HD2 $SA1 $SA2 $SA_DIFF

  # echo "Finished computing differences for $shape"
done









