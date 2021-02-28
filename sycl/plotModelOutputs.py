import matplotlib.pyplot as plt
import numpy as np
from decimal import Decimal
#TODO: Check if dependancies have to be recorded
#For now, run this script outside of the docker image to resolve python dependancies

#Extract C values
# with open('c.txt') as f: #grab our data
#     lines = f.readlines()
#     cT = [float(line.split()[0]) for line in lines]
#     cVal = [float(line.split()[2]) for line in lines]
    
with open('data/c.txt') as f: #grab expected data
    lines = f.readlines()
    cET = [float(line.split()[0]) for line in lines]
    cEVal = [float(line.split()[2]) for line in lines]

with open('data/c_noise.txt') as f: # grab our data generated with presence of noise
    lines = f.readlines()
    cNT = [float(line.split()[0]) for line in lines]
    cNVal = [float(line.split()[2]) for line in lines]

with open('data/sanitized_c.txt') as f: # grab our data generated with presence of noise
    lines = f.readlines()
    cST = [float(line.split()[0]) for line in lines]
    cSVal = [float(line.split()[2]) for line in lines]

print("Type of data: ", type(cET[0]))

chi = 0.0

#Find chi^2 value for c values
for i in range(len(cNVal)):
#chi = chi + Decimal pow((Decimal(cVal[i])-Decimal(cEVal[i])),2)/Decimal(cEVal[i])
    O = cNVal[i]
    E = cEVal[i]
    if(E!= 0):
        chi = chi + float (pow((O-E), 2.0)/E)

print('Chi-squared Value:', chi)

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
# ax1.plot(cT,cVal, c='r', label='Our output')
ax1.plot(cET,cEVal, c='b', label='Expected output')
ax1.plot(cNT,cNVal, c='g', label='Our output (with Gaussian Noise)')
ax1.plot(cST,cSVal, c='r', label='Our sanitized output')

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
    