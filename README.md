# Mitos

Mitos is a library and a tool for collecting sampled memory
performance data to view with
[MemAxes](https://github.com/scalability-llnl/MemAxes)

----

# Quick Start

## Requirements

Mitos requires:

* A Linux kernel with perf_events support for memory
  sampling.  This originated in the 3.10 Linux kernel, but is backported
  to some versions of [RHEL6.6](https://www.redhat.com/promo/Red_Hat_Enterprise_Linux6/).

* [Dyninst](http://www.dyninst.org) version 8.2 or higher.

* [hwloc](http://www.open-mpi.org/projects/hwloc/)

## Building

1. Make sure that Dyninst is installed and its location is added to the
   `CMAKE_PREFIX_PATH` environment variable.

2. Run the following commands from the root of the MemAxes source:
   ```
   mkdir build && cd build
   cmake -DCMAKE_INSTALL_PREFIX=/path/to/install/location ..
   make
   make install
   ```

## Running

### Mitosrun

1. Find the `mitosrun` command in the `bin` directory in the install
   directory.

2. Run any binary with `mitosrun` like this to generate a folder of
   mitos output data. For example:

   ```
   mitosrun ./examples/matmul
   ```

   The above command will run the matmul example and create a folder
   called mitos_###, where ### is the number of seconds since the
   epoch. The folder will contain:

   ```
   mitos_###/
      data/
         samples.csv
      src/
         <empty>
      hardware.xml
   ```

   Where `samples.csv` contains a comma-separated list of memory
   samples, hardware.xml describes the hardware topology (using hwloc)
   and src is an empty directory where you can put the program source
   files for use in MemAxes.

   `mitosrun` can also be fine-tuned with the following parameters:

   ```
   [options]:
       -b sample buffer size (default 4096)
       -p sample period (default 4000)
       -t sample latency threshold (default 10)
   ```

## IBS Configuration
1. Configure CMAKE with IBS depending on the chosen executable and configure environment variables if necessary:
* Mitosrun (with or without OpenMP): IBS_ALL_ON or IBS_THREAD_MIGRATION
* Mitoshooks with OpenMP: 
  * IBS_ALL_ON, requires currently Clang due to omp-tools.h dependency
  * Configure environment variable `OMP_TOOL_LIBRARIES` that points to mitoshooks-library:
    * OMP_TOOL_LIBRARIES=./../src/libmitoshooks.so
* Mitoshooks with MPI: IBS_THREAD_MIGRATION

# Authors

Mitos and MemAxes were originally written by Alfredo Gimenez.

Thanks to Todd Gamblin for suggestions and for giving Mitos a proper build setup.

# License

Mitos is distributed under the Apache-2.0 license with the LLVM exception.
All new contributions must be made under this license. Copyrights and patents
in the Mitos project are retained by contributors. No copyright assignment is
required to contribute to Mitos.

See [LICENSE](https://github.com/llnl/mitos/blob/develop/LICENSE) and
[NOTICE](https://github.com/llnl/mitos/blob/develop/NOTICE) for details.

SPDX-License-Identifier: (Apache-2.0 WITH LLVM-exception)

`LLNL-CODE-838491`
