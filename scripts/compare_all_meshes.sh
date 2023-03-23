#!/bin/bash
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

tri_force_shape="
subtract(){
  join(){
    plane(0,0,0, 1,1,0, 10);
    plane(0,0,0, 0,1,1, 10);
    plane(0,0,0, 1,0,1, 10);

    plane(10,10,10, -1,-1,-0, 10);
    plane(10,10,10, -0,-1,-1, 10);
    plane(10,10,10, -1,-0,-1, 10);
  };

  join(){
    plane(0,0,0, 1,2,0, 10);
    plane(0,0,0, 0,1,2, 10);
    plane(0,0,0, 2,0,1, 10);  

    plane(10,10,10, -2,-1,-0, 10);
    plane(10,10,10, -0,-2,-1, 10);
    plane(10,10,10, -1,-0,-2, 10);
  };
};
"

default_res="0.25,0.25,0.25"

declare -a shape_labels=("Sphere(10)" "aa_box(1.0,2.0,4.0)" "tri-force" "chamfered(box-sphere)")
declare -a shapes=("Sphere(10)" "aa_box(1.0,2.0,4.0)" "$tri_force_shape" "chamfer_subtract(2.0){aa_box(8.0,20.0,20.0);sphere(12.0);}")
declare -a resolutions=("$default_res" "0.25,0.25,0.75" "0.25,0.25,0.75" "$default_res")

printf "| %-20s | %-13s | %-13s | %-15s | %-15s | %-10s | %-15s | %-15s | %-10s| \n\n" Shape "Hausdorff 1" "Hausdorff 2" "Surface Area 1" "Surface Area 2" "Area Diff" "Volume 1" "Volume 2" "Volume Diff"

for i in ${!shapes[@]}
do
  shape=${shapes[$i]}
  res=${resolutions[$i]}
  shape_label=${shape_labels[$i]}
  # echo "Computing differences for $shape"
  OUTPUT="$(dicomautomaton_dispatcher \
    -v \
    -o GenerateMeshes \
      -p resolution=$res \
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
  SA_DIFF_LINE=$(grep "SURFACE AREA (%) difference" <<< "$OUTPUT")
  VOL_LINE=$(grep "VOLUME: First mesh" <<< "$OUTPUT")
  VOL_DIFF_LINE=$(grep "VOLUME (%) difference" <<< "$OUTPUT")


  HD1=$(sed -r "s/.*([0-9]+\.[0-9]+) or.*/\1/g" <<< $HD_LINE)
  HD2=$(sed -r "s/.*or ([0-9]+\.[0-9]+).*/\1/g" <<< $HD_LINE)

  SA1=$(sed -r "s/.*First mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $SA_LINE)
  SA2=$(sed -r "s/.*second mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $SA_LINE)

  SA_DIFF=$(sed -r "s/.*difference: (-?[0-9]+\.[0-9]+).*/\1/g" <<< $SA_DIFF_LINE)

  V1=$(sed -r "s/.*First mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $VOL_LINE)
  V2=$(sed -r "s/.*second mesh = ([0-9]+\.[0-9]+).*/\1/g" <<< $VOL_LINE)

  V_DIFF=$(sed -r "s/.*difference: (-?[0-9]+\.[0-9]+).*/\1/g" <<< $VOL_DIFF_LINE)

  # echo "$HD1"
  # echo "$HD2"

  printf "| %-20s | %-13.3f | %-13.3f | %-15.3f | %-15.3f | %-10.3f | %-15.3f | %-15.3f | %-10.3f |\n\n" $shape_label $HD1 $HD2 $SA1 $SA2 $SA_DIFF $V1 $V2 $V_DIFF


  # echo "Finished computing differences for $shape"
done









