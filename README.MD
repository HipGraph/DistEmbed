#DF2V
This is the official implementation for "DF2V: Distributed Memory Algorithms
for Force-Directed Graph Embedding".

## System Requirements
Users need to have the following softwares/tools installed in their PC/server. The source code was compiled and run successfully in NERSC Perlmutter.

```
GCC version >= 4.9 
OpenMP version >= 4.5
[ComBLAS](https://github.com/PASSIONLab/CombBLAS)
CMake version = 3.17
```

Some helpful links for installation can be found at [GCC](https://gcc.gnu.org/install/), [OpenMP](https://clang-omp.github.io) and [Environment Setup](http://heather.cs.ucdavis.edu/~matloff/158/ToolsInstructions.html#compile_openmp).

If you would like to use python scripts to get visualization, you need to have following python packages:
```
scikit-learn v0.22.2
networkx v2.3
scipy v1.3.1
numpy v1.16.2
matplotlib v3.1.1
python-louvain v0.13
```

## Compile DF2V
To compile DF2V, type the following command on terminal:
```
$ git clone git@github.com:HipGraph/DistEmbed.git
$ cd DistEmbed
$ mkdir build
$ cd build
$ cmake -DCMAKE_CXX_FLAGS="-fopenmp"  ..
$ make all
```
This will generate an executable file inside the bin folder names distembed.

## Users: Run DF2V from Command Line

Input file must be in matrix market format ([check here for details about .mtx file](https://math.nist.gov/MatrixMarket/formats.html)). A lot of datasets can be found at [suitesparse website](https://sparse.tamu.edu). We provide a sample input files inside the  datasets directory. To run DF2V in shared memory setup, type the following command:
```
$ ./bin/distembed -input ./datasets/minst.mtx -output ./datasets/output/ -iter 1200 -batch 256 
```
To run DF2V in distributed memory setup check the job submission batch script [mnist_job.sh](datasets/minst_job.sh)'

Here, `-input` is the full path of input file, `-output` is the directory where output/embedding file will be saved, `-iter` is the number of iterations, `-batch` is the size of minibatch which is 256 here. All options are described below:
```
-input <string>, full path of input file (required).
-output <string>, directory where output file will be stored. (default: current directory)
-batch <int>, size of minibatch. (default:384)
-iter <int>, number of iteration. (default:1200)
-nsamples <int>, number of negative samples. (default:5)
-lr <float>, learning rate of SGD. (default:0.02)
-alpha <double> [0,1], decides number of processors involving in pushing and pulling
-beta <double> [0,1] decides the chunk size of single communication and computation overlap.
-sync_comm <int> {0,1} 0 indicates asynchornouse communication and 1 indicates synchronouse communication.
```
First line of output file will contains the number of vertices (N) and the embedding dimension (D). The following N lines will contain vertex id and a D-dimensional embedding for corresponding vertex id.

## Generate 2D Visualizations of an Embedding ##
To generate 2D visualiation  run the following command which will generate a PDF file in the current directory:
```
$ python -u ./datasets/runvisualization.py datasets/mnist.mtx 1 datasets/output/embedding.txt 2 datasets/minst.labels.txt.nodes mnistgraph
```

## Citation
If you find this repository helpful, please cite our papers as follows:


## Contact
This repository is maintained by Isuru Ranawaka. If you have questions, please ping me at `isjarana@iu.edu` or create an issue.