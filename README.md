# kdstinker19
This will dump any driver loaded using intel driver (2019 one)

# Why
The intel driver (used with kdmapper) from 2019 is not calling iocreatedevice but iocreatedevicesecure. 
They manually resolve this function with MmGetSystemRoutineAddress, thus not present in IAT table.

# How
https://githacks.org/Shawick/goodeye - IAT hooks
https://githacks.org/Shawick/kdstinker - Idea
