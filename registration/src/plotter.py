import matplotlib.pyplot as plt
import mpl_toolkits.mplot3d.axes3d as p3
import matplotlib.animation as manimation
import os
import re

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

def natural_sort(l): 
    convert = lambda text: int(text) if text.isdigit() else text.lower() 
    alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ] 
    return sorted(l, key = alphanum_key)

def write_vid_frame(stationary_file, moving_file, ax, writer):
    print(moving_file)
    x1,y1,z1 = read_file(stationary_file)
    x2,y2,z2 = read_file(moving_file)

    iteration = re.findall(r'\d+', moving_file)

    ax.cla()
    ax.scatter(x1, y1, z1, s=10, c='b', marker="s", label='Original')
    ax.scatter(x2, y2, z2, s=10, c='r', marker="o", label='Translated')
    plt.legend(loc='upper left')

    if len(iteration) > 0:
        ax.set_title("Iteration " + iteration[0])
    else: 
        ax.set_title("Final alignment")
    # plt.show()
    writer.grab_frame()

def main():
    stationary_file = "../../3D_Printing/20201119_Registration_Phantom/reg_phant_v0_original.xyz"
    moving_folder = "../../Output/Phantom_Nonrigid_Rot"
    file_output = "../../Output/phantom_nonrigid_rot.mp4"

    FFMpegWriter = manimation.writers['ffmpeg']
    metadata = dict(title='Movie Test', artist='Matplotlib',
            comment='Movie support!')
    writer = FFMpegWriter(fps=8, metadata=metadata)

    for root, dirs, files in os.walk(moving_folder):
        file_list = natural_sort(files)

    fig = plt.figure()
    ax = p3.Axes3D(fig)

    with writer.saving(fig, file_output, 100):
        for moving_file in file_list:
            write_vid_frame(stationary_file, os.path.join(root,moving_file), ax, writer)

if __name__ == '__main__':
    main()
