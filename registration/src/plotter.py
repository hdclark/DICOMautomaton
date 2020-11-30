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
    ax1.scatter(x1, y1, z1, s=10, c='b', marker="s", label='first')
    ax1.scatter(x2, y2, z2, s=10, c='r', marker="o", label='second')
    plt.legend(loc='upper left')
    plt.show()

def main():
    file_name_1 = "data_files/reg_phant_v0_original.xyz"
    file_name_2 = "data_files/reg_phant_v0_rotated.xyz"
    x1,y1,z1 = read_file(file_name_1)
    x2,y2,z2 = read_file(file_name_2)
    plot_points(x1,y1,z1,x2,y2,z2)

if __name__ == '__main__':
    main()
