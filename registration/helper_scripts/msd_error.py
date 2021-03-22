import argparse
import numpy as np

def read_file(file_name):
    with open(file_name, "r") as fp:
        points = []

        for line in fp:
            line_data = line.split()
            if len(line_data) == 3:
                points.append(np.array(line_data))

    return np.array(points).astype(np.float)

def main():
    parser = argparse.ArgumentParser(
        description="File I/O specification for plotter")
    parser.add_argument("file1", help="Path to first point cloud file")
    parser.add_argument("file2", help="Path to second point cloud file")

    args = parser.parse_args()
    point_set_1 = read_file(args.file1)
    point_set_2 = read_file(args.file2)

    error = np.square(point_set_1 - point_set_2).sum()/point_set_1.shape[0]
    print("Error: " + str(error))

if __name__ == '__main__':
    main()
