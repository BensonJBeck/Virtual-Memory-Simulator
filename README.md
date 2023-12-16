# Virtual-Memory-Simulator
This project was written in my operating systems course in college. The following summarizes my findings using this program, and includes a brief overview of how to use it. 
## 🔌 Usage 🔌
*Compile the vsim.c file, and note the binary name...*

You can run the program with the following argument usage :

**"./executable-name –n <numframes> -a <opt|clock|lru> <tracefile>"**

- "executable-name" : the name of whatever you chose to output the compiled "vsim.c" file to
- "numframes" : the number of simulated RAM frames which should be used for the simulation
- "< opt | clock | lru >" : the name of the page replacement algorithm that should be used when handling page faults
- "tracefile" : the name of the .trace file (generated using a valgrind tool) which will be used for the simulation

Upon program execution, within the current directory, a "stat.txt" file will be generated/overwritten with the following statistics :

- Algorithm: %s
- Number of frames: %d
- Total memory accesses: %d
- Total page faults: %d
- Total writes to disk: %d
- Number of page table leaves: %d
- Total size of page table: %d
## 💥 Results 💥
The following experiment was run on a M1 Macbook Pro, and tested using the provided “ls.trace” file dataset. Each set of statistics were recorded in about 2 seconds max, and the source code may be observed in the attached “vsim.c” file. Data is first read into a queue structure and marked with the pages “next use” upon program execution. The next use statistic is representative of when the page the instruction references will be used again, this information is necessary only for the optimal page replacement alogrithm’s implementation but is marked regardless of the algorithm requested. Once preprocessing is complete, the simulation is able to run. It sequentially moves through the instruction within the queue and updates the underlying page table data structure as needed. The experiment was run with 8, 16, 32, and 64 simulated frames of RAM using optimal page replacement (OPT), last recently used page replacement (LRU), and clock second chance page replacement (CLOCK). With this in mind, the following statistics were recorded.

### 👉 notes 👈
A multi-level page table with a root containing 512 leaf entries is used during this simulation. 32-bit virtual addresses are indexed with the upper 9-bits for the first level index, and the following 11 bits for the second level index. Offsets are disregarded in this simulation, as it's assumed every address is "valid". The following results were generated using the first version of this software, and results may not be accurate! These were my findings, with the provided code. 

## Page Faults
| # frames | OPT | LRU | CLOCK |
| ----------- | ----------- |----------- |----------- |
| 8 Frames | 254 | 71057 | 558 |
| 16 Frames | 56 | 37315 | 181 |
| 32 Frames | 19 | 18885 | 34 |
| 64 Frames | 6 | 10647 | 3 |
## Writes to Disk
| # frames | OPT | LRU | CLOCK |
| ----------- | ----------- |----------- |----------- |
| 8 Frames | 577 | 248712 | 790 |
| 16 Frames | 158 | 156682 | 308 |
| 32 Frames | 5 | 52086 | 9 |
| 64 Frames | 4 | 27617 | 4 |
## Final Leaves in the Page Table
| # frames | OPT | LRU | CLOCK |
| ----------- | ----------- |----------- |----------- |
| 8 Frames | 3 | 2 | 3 |
| 16 Frames | 3 | 2 | 3 |
| 32 Frames | 3 | 3 | 4 |
| 64 Frames | 3 | 4 | 4 |





