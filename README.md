## Overview

This project implements a user-level threading library that has a first come first serve thread scheduler capable of handling many-to-many threading. It has two types of executors: one for compute tasks and another for I/O tasks. The I/O executor offloads I/O operations to prevent blocking. 
## Usage

To compile and run the provided code, follow these steps:

```bash
# clone the repository
git clone https://github.com/eleadufresne/Thread-Scheduler.git
# navigate to the repository directory
cd Thread-Scheduler
# compile the code
make
# run the program
./sut
```

Optionally, you can clean up the generated executable and object files.
```bash
make clean
```
