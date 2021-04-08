import argparse
import numpy as np
import csv
import os

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
    parser.add_argument("stats_file", help="Path to stats file")
    parser.add_argument("point_file", help="Path to stationary point cloud file")

    args = parser.parse_args()
    stats = args.stats_file
    stationary = args.point_file

    stats_tmp = stats + ".tmp"

    with open(stats, 'r') as csvinput:
        with open(stats_tmp, 'w') as csvoutput:
            writer = csv.writer(csvoutput)
            for row in csv.reader(csvinput):
                moving = "../" + row[3]
                point_set_1 = read_file(moving)
                point_set_2 = read_file(stationary)
                error = (np.square(point_set_1 - point_set_2).sum()/point_set_1.shape[0])
                writer.writerow(row+[str(error)])
    os.rename(stats_tmp, stats)

if __name__ == '__main__':
    main()
