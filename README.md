# Huron: Hybrid False Sharing Detecetion and Repair

This repository contains the source code implementation of our paper, [Huron: Hybrid False Sharing Detection and Repair](https://web.eecs.umich.edu/~barisk/public/huron.pdf). While the implementation can be built from this repository manually, we strongly recommend the following step by step guide using a pre-installed virtual machine.

## Pre-requisite
1. The supplied virtual machine disk (VMDK) file can be run on [VirtualBox software](https://www.virtualbox.org). We have tested the VMDK file on VirtualBox 5.2.26 r128414 (Ubuntu 16.04) and VirtualBox-6.0.4-128413 (Windows 10) but hope the artifact will work on other versions of VirtualBox as well. To view VirtualBox’s version number you can go to Help -> About VirtualBox.
2. Some of the benchmarks have high memory usage and they become memory-bounded unless the system has at least 4GB of RAM in virtual machine. Therefore, we recommend that the testing machine has at least 8GB of RAM.
3. Since false sharing only happen in shared-memory multi-processors, our evaluation requires at-least 4 logical processor cores in the virtual machine. Therefore, we recommend that the testing machine has at-least 8 logical processor cores.
4. When decompressed, our virtual machine disk is about 23.4GB. Therefore, we recommend that the testing machine has at least 30 GB of free hard disk space.

## Getting Started
1. Download the compressed (tar.gz format) virtual machine disk (VMDK) file from [here](https://drive.google.com/file/d/1VqxgpKb_AaZHpXSNK0NqK0V0SiYqVR5j/view?usp=sharing).
2. Extract the tar.gz file using, `tar -xvzf paper657.tar.gz`command. For windows, please follow the procedure described [here](https://www.simplehelp.net/2015/08/11/how-to-open-tar-gz-files-in-windows-10/).
3. Now, open VirtualBox and import the VMDK file by following the instructions [here](https://medium.com/riow/how-to-open-a-vmdk-file-in-virtualbox-e1f711deacc4). For the name and operating system of the virtual machine, use the below options
	> Name: PLDI-19
	> Type: Linux
	> Version: Other Linux (64-bit)
For memory size, use 4096 MB (4GB).
4. Before starting the virtual machine, change the number of processor cores to 4 following the instructions from [here](https://www.youtube.com/watch?v=42769_AGbx8).
5. Now, start the virtual machine by double-clicking PLDI-19 group on the left sidebar.
6. Once the system boots up, log in to the PLDI-19 user. The username/password for the VM is `PLDI-19`/`PLDI-19`. However, you should not need to use this information. The VM automatically logs in and will never lock after inactivity or sleep, and nothing in this guide requires root permissions. The original Lubuntu image was downloaded from the osboxes.org website. Therefore, `osboxes.org` (or just `osboxes`) username would also work. The password is the same (`PLDI-19`) for this user as well.
7. Once logged in, start the terminal by pressing (Ctrl+Alt+t). If the keyboard shortcut, does not work for you, follow the instructions [here](https://askubuntu.com/questions/124274/how-to-find-the-terminal-in-lubuntu).
8. In the terminal, execute the following commands (lines that start with `$`, omit the `$` while running). Lines starting with # are comment for your convenience, please don’t type them into the command line.
```
$ cd pldi-19
$ ls
huron-repair llvm sheriff-master
$ cd huron-repair # you can ls to see the contents of huron after this command
$ cd test_suites # you can ls to see after this command as well if you like
$ cd histogram
$ bash run.sh
This file has 1500000000 bytes of image data, 500000000 pixels
Starting pthreads histogram
This file has 1500000000 bytes of image data, 500000000 pixels
Starting pthreads histogram
we're gonna begin now.
This file has 1500000000 bytes of image data, 500000000 pixels
Starting pthreads histogram
This file has 1500000000 bytes of image data, 500000000 pixels
Starting pthreads histogram
$ cat time.csv
original, 16968
product, 3195
sheriff, 3649
manual, 3133
```

Congratulations! You have successfully run your first `Huron`-ed program, `product.out`. The `run.sh` script also runs the original benchmark program histogram, `original.out` as well as the version where [Sheriff](https://github.com/plasma-umass/sheriff) repaired false sharing, `sheriff.out`, and manually repaired version, `manual.out`. All the execution times (in milliseconds) are then logged in `time.csv` file.
