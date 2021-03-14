## Perfusion SCDI Model

This folder includes two closed-form perfusion models: single compartment single input(SCSI) and single compartment dual input(SCDI). The SCSI model is an implementation of the work of Zeng et al: https://escholarship.org/uc/item/8145r96 and the SCDI model is inspired by the same paper but uses an extra equation to solve for the extra kinetic parameter (K1V). The link to the paper explaining this model in detail will be added. 

The sanitization code (sanitizeInputData.py) is also included that takes in the files needed to be sanitized and saves them in the data folder. The path to the input files and for the output files should be changed before running the script. We recommend that you sanitize all input files before running the model.

### Running the Model
To compile the code run:
`./compile_and_install.sh`
To run Perfusion SCDI with one c file compile and then run: 
```
    run_model -h -a /path/to/aif.txt -v /path/to/vif.txt -c /path/to/c.txt
```
To run Perfusion SCDI with multiple c files, store all c files in the same folder with the format `c*.txt`, then run:
```
    run_model -h -a /path/to/aif.txt -v /path/to/vif.txt /path/to/c*.txt
```
The kinetic parameters will then be stored in a file named `kParams_SCDI_timestamp.txt`. Each row corresponds to one C file based on the order of the C files. The columns from left to right are `k1_A`, `k1_V`, and `k2`.

To run Perfusion SCSI you can add the `-b` flag and run the commands in the same manner. You still do need to import the VIF file but it will not be used in the model. The kinetic parameters then are stored in a file named `kParams_SCSI_timestamp.txt`. The columns are `k1` and `k2` respectively. 
 ### Development 
 To visualize the output model, we have created a program similar to `gen_test_inputs` that uses real `AIF` and `VIF` files and the kinetic parameters to generate a time series C file. The input C file and the output C file then can be compared to validate the model. The K parameters have to be manually changed in the file `Generate_Expected_Output.cc` and the AIF and VIF files are arguments. Simply run:
 ```
    gen_expected_output -a /path/to/aif.txt -v /path/to/vif.txt
 ```
