import argparse
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def read_file(file_name):
    xyz = open(file_name, "r")

    x_points = []
    y_points = []
    z_points = []

    for line in xyz:
        line_data = line.split()
        if len(line_data) == 3:
            x, y, z = line_data
            x_points.append(float(x))
            y_points.append(float(y))
            z_points.append(float(z))

    xyz.close()
    return x_points, y_points, z_points

def plot_points(x1,y1,z1,x2,y2,z2):
    fig = plt.figure()
    ax1 = fig.add_subplot(111, projection = '3d')
    ax1.scatter(x1, y1, z1, s=1, c='b', marker="s", label='Stationary Dataset')
    ax1.scatter(x2, y2, z2, s=1, c='r', marker="o", label='Moving Dataset')
    ax1.set_axis_off()
    ax1.view_init(130, -130)
    # ax1.view_init(-80, 100) #anteater
    plt.legend(loc='upper left')
    plt.show()

def main():
    parser = argparse.ArgumentParser(
        description="File I/O specification for plotter")
    parser.add_argument("stationary", help="Path to stationary point cloud")
    parser.add_argument("moving", help="Path to moving point cloud files")

    args = parser.parse_args()

    file_name_1 = args.stationary
    file_name_2 = args.moving
    x1,y1,z1 = read_file(file_name_1)
    x2,y2,z2 = read_file(file_name_2)
    plot_points(x1,y1,z1,x2,y2,z2)

if __name__ == '__main__':
    main()
