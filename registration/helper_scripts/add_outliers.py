import argparse
import numpy as np

def read_file(file_name):
    with open(file_name, "r") as fp:
        points = []

        for line in fp:
            line_data = line.split()
            if len(line_data) == 3:
                points.append(np.array(line_data))

    return np.array(points)

def add_outliers(full_point_set, val_range, percentage):
    num_points = full_point_set.shape[0]
    num_points_to_add = int(percentage * num_points)
    max_val = np.amax(full_point_set)
    min_val = np.amin(full_point_set)
    outliers = np.random.normal(scale=val_range,
                                size=(num_points_to_add, 3))
    new_updated_point_set = np.concatenate((full_point_set, outliers), axis=0)
    return new_updated_point_set


def write_file(file_name, point_set):
    with open(file_name, "w") as fp:
        for point in point_set:
            line = "%s %s %s\n" % (point[0], point[1], point[2])
            fp.write(line)

def main():
    parser = argparse.ArgumentParser(
        description="File I/O specification for plotter")
    parser.add_argument("infile", help="Path to moving point cloud files")
    parser.add_argument(
        "range", help="Range of outliers", type=float)
    parser.add_argument(
        "percentage", help="Ratio of outliers to valid points", type=float)
    parser.add_argument("outfile", help="Path to save file")

    args = parser.parse_args()
    full_point_set = read_file(args.infile)
    outlier_point_set = add_outliers(
        full_point_set.astype(np.float), args.range, args.percentage)
    write_file(args.outfile, outlier_point_set)

if __name__ == '__main__':
    main()
