## Difficulties with Partial Unrolling

1. How can we know where to start the loop and where to end it? `for i = 0 to 100` doesnot create any problem, however, `for i = a[b] to a [c]` will create a problem. How can we identify this start address.

2. How can we make thread distributed loops deterministic? For example, 
```
lock.lock()
for x[next] to x[next+dist]
  do something
next = next+dist+1
lock.unlock()
```
What happens here due to race condition? Intuitive solution: Assign priorities among threads.

3. How to identify non-logged instruction pointers where falsely shared memory address is accessed? We started logging a memory location only when its corresponding cacheline is accessed by at least two threads and one of them is writing to it. So there are some memory accesses to those false shared memory address that was not logged during our first pass. How do we identify them at the second pass. Intuitive solution: Run three pass, on the second pass identify these locations.
