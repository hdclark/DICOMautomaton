import matplotlib.pyplot as plt
import numpy as np
#TODO: Check if dependancies have to be recorded
#For now, run this script outside of the docker image to resolve python dependancies

#Extract C values
with open('c.txt') as f: #grab our data
    lines = f.readlines()
    cT = [line.split()[0] for line in lines]
    cVal = [line.split()[2] for line in lines]
    
with open('data/c.txt') as f: #grab expected data
    lines = f.readlines()
    cET = [line.split()[0] for line in lines]
    cEVal = [line.split()[2] for line in lines]

# #Extract AIF values
# with open('aif.txt') as f: #grab our data
#     lines = f.readlines()
#     aifT = [line.split()[0] for line in lines]
#     aifVal = [line.split()[2] for line in lines]
    
# with open('data/aif.txt') as f: #grab expected data
#     lines = f.readlines()
#     aifET = [line.split()[0] for line in lines]
#     aifEVal = [line.split()[2] for line in lines]

# #Extract VIF values
# with open('vif.txt') as f: #grab our data
#     lines = f.readlines()
#     vifT = [line.split()[0] for line in lines]
#     vifVal = [line.split()[2] for line in lines]
    
# with open('data/vif.txt') as f: #grab expected data
#     lines = f.readlines()
#     vifET = [line.split()[0] for line in lines]
#     vifEVal = [line.split()[2] for line in lines]


fig = plt.figure()

ax1 = fig.add_subplot(1,1,1)
ax1.set_title("C")    
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Intensity')
ax1.plot(cT,cVal, c='r', label='Our output')
ax1.plot(cET,cEVal, c='b', label='Expected output')

# ax1 = fig.add_subplot(1,3,2)
# ax1.set_title("AIF")    
# ax1.set_xlabel('Time (s)')
# ax1.set_ylabel('Intensity')
# ax1.plot(aifT,aifVal, c='r', label='Our output')
# ax1.plot(aifET,aifEVal, c='b', label='Expected output')

# ax1 = fig.add_subplot(1,3,3)
# ax1.set_title("VIF")    
# ax1.set_xlabel('Time (s)')
# ax1.set_ylabel('Intensity')
# ax1.plot(vifT,vifVal, c='r', label='Our output')
# ax1.plot(vifET,vifEVal, c='b', label='Expected output')

leg = ax1.legend()

plt.show()
    