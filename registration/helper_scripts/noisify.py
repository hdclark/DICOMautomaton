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

def add_gaussian_noise(full_point_set, scale, sigma):
    print(scale)
    print(np.amax(full_point_set) - np.amin(full_point_set))
    scale = scale * (np.amax(full_point_set) - np.amin(full_point_set))
    print(scale)
    noise = scale * np.random.normal(scale=sigma, size=full_point_set.shape)
    return full_point_set + noise

def write_file(file_name, point_set):
    with open(file_name, "w") as fp:
        for point in point_set:
            line = "%s %s %s\n" % (point[0], point[1], point[2])
            fp.write(line)

def main():
    parser = argparse.ArgumentParser(
        description="File I/O specification for plotter")
    parser.add_argument("infile", help="Path to moving point cloud files")
    parser.add_argument("outfile", help="Path to save file")
    parser.add_argument(
        "-scale", help="Value to scale Gaussian noise by", type=float, default=1)
    parser.add_argument(
        "-sigma", help="Percentage of points to remove", type=float, default=1)

    args = parser.parse_args()
    full_point_set = read_file(args.infile)
    noisy_point_set = add_gaussian_noise(
        full_point_set, args.scale, args.sigma)
    write_file(args.outfile, noisy_point_set)

if __name__ == '__main__':
    main()
