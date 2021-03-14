import numpy as np #importing numpy module for efficiently executing numerical operations
import matplotlib.pyplot as plt #importing the pyplot from the matplotlib library
from scipy import signal

with open('data/input/c_noise.txt') as f:
    lines = f.readlines()
    c_t = [float(line.split()[0]) for line in lines]
    c_i = [float(line.split()[2]) for line in lines]

with open('data/input/aif_noise.txt') as f:
    lines = f.readlines()
    a_t = [float(line.split()[0]) for line in lines]
    a_i = [float(line.split()[2]) for line in lines]

with open('data/input/vif_noise.txt') as f:
    lines = f.readlines()
    v_t = [float(line.split()[0]) for line in lines]
    v_i = [float(line.split()[2]) for line in lines]

# - - - # We load the data in the mat format but this code will work for any sort of time series.# - - - #
cT = np.array(c_t)
cI = np.array(c_i)
aT = np.array(a_t)
aI = np.array(a_i)
vT = np.array(v_t)
vI = np.array(v_i)

# Visualizing the original and the Filtered Time Series
fig = plt.figure()
ax = fig.add_subplot(1, 1, 1)
ax.plot(aT,aI,'k-',lw=0.5)

## Filtering of the time series
fs=1/1.2 #Sampling period is 1.2s so f = 1/T

nyquist = fs / 2 # 0.5 times the sampling frequency
cutoff=0.25 # fraction of nyquist frequency
b, a = signal.butter(5, cutoff, btype='lowpass') #low pass filter

cIfilt = signal.filtfilt(b, a, cI)
cIfilt=np.array(cIfilt)
cIfilt=cIfilt.transpose()

aIfilt = signal.filtfilt(b, a, aI)
aIfilt=np.array(aIfilt)
aIfilt=aIfilt.transpose()

vIfilt = signal.filtfilt(b, a, vI)
vIfilt=np.array(vIfilt)
vIfilt=vIfilt.transpose()

# write filtered data back into a txt file
zeroArr = np.zeros((cT.shape[0],), dtype=int)
Cdata = np.column_stack((cT,zeroArr, cIfilt, zeroArr))
Adata = np.column_stack((aT,zeroArr, aIfilt, zeroArr))
Vdata = np.column_stack((vT,zeroArr, vIfilt, zeroArr))

with open('data/input/sanitized_c.txt', 'w') as txt_file:
    txt_file.write('0.0 0 0.0 0\n')
    for row in Cdata:
        line = ' '.join(str(v) for v in row)
        txt_file.write(line + "\n") # works with any number of elements in a line

with open('data/input/sanitized_aif.txt', 'w') as txt_file:
    txt_file.write('0.0 0 0.0 0\n')
    for row in Adata:
        line = ' '.join(str(v) for v in row)
        txt_file.write(line + "\n") # works with any number of elements in a line

with open('data/input/sanitized_vif.txt', 'w') as txt_file:
    txt_file.write('0.0 0 0.0 0\n')
    for row in Vdata:
        line = ' '.join(str(v) for v in row)
        txt_file.write(line + "\n") # works with any number of elements in a line

ax.plot(aT, aIfilt, 'b', linewidth=1)
ax.set_xlabel('Time (s)',fontsize=18)
ax.set_ylabel('Contrast Enhancement Intensity',fontsize=18)
plt.show()