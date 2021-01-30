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

def random_downsample(full_point_set, percentage):
    num_points = full_point_set.shape[0]
    num_points_to_keep = int((1-percentage) * num_points)
    indices = np.arange(num_points)
    indeices_to_keep = np.random.choice(
        indices, size=num_points_to_keep, replace=False)
    return full_point_set[indeices_to_keep]

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
        "ds_rate", help="Percentage of points to remove", type=float)
    parser.add_argument("outfile", help="Path to save file")

    args = parser.parse_args()
    full_point_set = read_file(args.infile)
    downsampled_point_set = random_downsample(full_point_set, args.ds_rate)
    write_file(args.outfile, downsampled_point_set)

if __name__ == '__main__':
    main()
