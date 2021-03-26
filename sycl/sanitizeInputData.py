import numpy as np  # importing numpy module for efficiently executing numerical operations
import matplotlib.pyplot as plt  # importing the pyplot from the matplotlib library
from scipy import signal
import sys, getopt

def main(argv):
    cfreqPercent = 0.18
    try:
        opts, args = getopt.getopt(argv,"h:c:",["cfreq="])
    except getopt.GetoptError:
        print('sanitizeInputData.py -h <help> -c <cutoff percentage (decimagl value from 0 to 1)>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print('sanitizeInputData.py -h <help> -c <cutoff percentage (decimagl value from 0 to 1)>')
            sys.exit()
        elif opt in ("-c", "--cfreq"):
            cfreqPercent = arg
    print ('Percentage cutoff of Nyquist frequency: ', cfreqPercent)
    sanitizeData(float(cfreqPercent))

# Function to sanitize/filter the input data to the SCDI model
# Input: the percentage (expressed as a decimal between 0 and 1) by which to scale the max allowable freqency of 
# the time course when used as a cutoff frequency for a lowpass filter. Defaults to 0.18, as specified in main.
def sanitizeData(cfreqPercent):
    # Note: The file paths 'data/raw/...' should be replaced with the paths to the AIF, VIF, and C time courses
    # that are to be sanitized.
    with open('data/raw/C_000009.txt') as f:
        lines = f.readlines()
        c_t = [float(line.split()[0]) for line in lines]
        c_i = [float(line.split()[2]) for line in lines]

    with open('data/raw/AIF_000000.txt') as f:
        lines = f.readlines()
        a_t = [float(line.split()[0]) for line in lines]
        a_i = [float(line.split()[2]) for line in lines]

    with open('data/raw/VIF_000001.txt') as f:
        lines = f.readlines()
        v_t = [float(line.split()[0]) for line in lines]
        v_i = [float(line.split()[2]) for line in lines]

    # We load the data in the mat format but this code will work for any sort of time series.
    cT = np.array(c_t)
    cI = np.array(c_i)
    aT = np.array(a_t)
    aI = np.array(a_i)
    vT = np.array(v_t)
    vI = np.array(v_i)

    # Visualizing the original and the Filtered Time Series
    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1)
    ax.plot(c_t, c_i, 'k-', lw=0.5)

    # Apply low pass filter to time courses
    maxAllowableFreq_c = findMaxAllowableFrequency(cT)
    maxAllowableFreq_a = findMaxAllowableFrequency(aT)
    maxAllowableFreq_v = findMaxAllowableFrequency(vT)

    c_b, c_a = signal.butter(5, cfreqPercent*maxAllowableFreq_c, btype='lowpass')  # low pass filter for c
    a_b, a_a = signal.butter(5, cfreqPercent*maxAllowableFreq_a, btype='lowpass')  # low pass filter for a
    v_b, v_a = signal.butter(5, cfreqPercent*maxAllowableFreq_v, btype='lowpass')  # low pass filter for v

    cIfilt = signal.filtfilt(c_b, c_a, cI)
    cIfilt = np.array(cIfilt)
    cIfilt = cIfilt.transpose()

    aIfilt = signal.filtfilt(a_b, a_a, aI)
    aIfilt = np.array(aIfilt)
    aIfilt = aIfilt.transpose()

    vIfilt = signal.filtfilt(v_b, v_a, vI)
    vIfilt = np.array(vIfilt)
    vIfilt = vIfilt.transpose()

    # Write filtered data back into a txt file
    zeroArr = np.zeros((cT.shape[0],), dtype=int)
    Cdata = np.column_stack((cT, zeroArr, cIfilt, zeroArr))
    Adata = np.column_stack((aT, zeroArr, aIfilt, zeroArr))
    Vdata = np.column_stack((vT, zeroArr, vIfilt, zeroArr))

    # Save the filtered time courses to folder 'data/input'  
    with open('data/input/sanitized_c.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Cdata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    with open('data/input/sanitized_aif.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Adata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    with open('data/input/sanitized_vif.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Vdata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    ax.plot(cT, cIfilt, 'b', linewidth=1)
    ax.set_xlabel('Time (s)', fontsize=18)
    ax.set_ylabel('Contrast Enhancement Intensity', fontsize=18)
    plt.show()

# Function to find the "allowed frequencies" (frequencies that obey the Nyquist Sampling Condition) in the time course.
# Input: An array of times at which the time course was sampled
# 
# Note: Assuming that the original signal HAS BEEN sampled in accordance with the Nyquist condition (otherwise signal would alias),
# then, v>2*(f_max_of_the_signal).  To find the maximum freqency that should be allowed to pass through a low-pass filter 
# (used to get rid of noise), let us find f_max_of_the_signal and filter out any freqency above that.  To do this, find in the adaptively
# sampled signal the fastest/minimum (most restrictive on the Nyquist condition) sampling rate (v) and then solve for f_max_of_the_signal.
def findMaxAllowableFrequency(times):
    #Find the fastest sampling rate in the time course
    minSamplingRate = float('inf')
    for i in range(0,len(times)-1):
        diff = times[i+1] - times[i]
        minSamplingRate = min(diff, minSamplingRate)
    #Calculate and return the maximum allowed freq. in the sample
    return minSamplingRate/2
    
if __name__ == "__main__":
    main(sys.argv[1:])
