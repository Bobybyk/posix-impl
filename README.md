# POSIX locks implementation

*This project was carried out as part of my master's studies in fundamental computer science*

The goal of the project is to implement locks that allow segments to be locked of files, like POSIX locks, but avoiding the behavior described at the end of the previous section.
The problem, of course, is that we don't have access to the system's kernel. So we can't add anything to the file openings table.
The idea is to store the necessary data structures in shared memory obtained by the projection (mmap) of a shared memory object (SMO) (shm_open).
