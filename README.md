# CS546-Paxos
Readme
Intorduction:
	This is a simple program to mimic Paxos in select master node case.
How to run this program?
	After install MPICH-3.2, compile and run:( please use 5 node)
		mpicc -g  -w -o sm Select_matster_node.c
		mpiexec -n 5 ./sm TASK_NUM MODE
		MODE ==0: all node safe MODE ==1: have dead or lost message node
How to evaluate the result?
	I use an array memo to represent cache in each node. For each node in rank 0~4, their proposal is to change value on memo[rank] row. 
	At beginning, the memo in each node is like this:
Now data in memo is : in N, VAR, DATA state order
 -1 N 101 0
 -1 N 101 0
 -1 N 101 0
 -1 N 101 0
 -1 N 101 0
	If a node with rank i dead or lost message, m[i] should stay as initial state, only no error node can be the master node.
	After program ends, the result should stay same and it may looks like this:
 10 a 86 2
 -1 N 101 3
 -1 N 101 8
 18 d 86 2
 -1 N 101 3
	3 represent node is dead and 8 represent node is lost message.
What's more:
	Because have no access to Jarvis(can't compile and run on Jarvis), I run it using AWS. System: Ubuntu. They is unsolved bug when run on Ubuntu 11 with MPICH version more than 2, so it may affect the result when the task num increase.
	Please contact with wtang13@hawk.iit.edu for more information.
