import matplotlib.pyplot as plt
import mpl_toolkits.mplot3d.axes3d as p3
import matplotlib.animation as manimation
import argparse
import os
import re

def read_file(file_name):
    print(file_name)
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

def natural_sort(l): 
    convert = lambda text: int(text) if text.isdigit() else text.lower() 
    alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ] 
    return sorted(l, key = alphanum_key)

def write_vid_frame(stationary_file, moving_file, ax, writer):
    print(moving_file)
    x1,y1,z1 = read_file(stationary_file)
    x2,y2,z2 = read_file(moving_file)

    moving_split = moving_file.split("_")
    iteration = []
    similarity = []
    for item in moving_split:
        if "iter" in item:
            iteration = re.findall(r"[-+]?\d*\.\d+|\d+", item)
        if "sim" in item:
            similarity = re.findall(r"[-+]?\d*\.\d+|\d+", item)

    ax.cla()
    ax.scatter(x1, y1, z1, s=1, c='b', marker="s", label='Stationary Point Set')
    ax.scatter(x2, y2, z2, s=1, c='r', marker="o", label='Moving Point Set')
    plt.legend(loc='upper left')


    title = ""
    if len(iteration) > 0:
        title += "Iteration " + iteration[len(iteration)-1]
    else: title += "Final alignment"
    if len(similarity) > 0:
        title += ", Error: " + similarity[len(similarity)-1]

    ax.set_title(title, y=-0.01)
    plt.tight_layout()
    # plt.show()
    writer.grab_frame()

def main():
    parser = argparse.ArgumentParser(
        description="File I/O specification for plotter")
    parser.add_argument("stationary", help="Path to stationary point cloud")
    parser.add_argument("moving", help="Path to moving point cloud files")
    parser.add_argument("output", help="Path to save file")

    args = parser.parse_args()

    FFMpegWriter = manimation.writers['ffmpeg']
    metadata = dict(title='Movie Test', artist='Matplotlib',
            comment='Movie support!')
    writer = FFMpegWriter(fps=4, metadata=metadata)

    for root, dirs, files in os.walk(args.moving):
        file_list = natural_sort(files)

    fig = plt.figure()
    # ax = fig.add_subplot(111)
    ax = p3.Axes3D(fig)
    # ax.view_init(90, 270)

    with writer.saving(fig, args.output, 100):
        for i in range(5):
            moving_file = file_list[0]
            write_vid_frame(
                args.stationary, os.path.join(root, moving_file), ax, writer)
        for moving_file in file_list:
            write_vid_frame(
                args.stationary, os.path.join(root, moving_file), ax, writer)
        for i in range(5):
            moving_file = file_list[-1]
            write_vid_frame(
                args.stationary, os.path.join(root, moving_file), ax, writer)

if __name__ == '__main__':
    main()
