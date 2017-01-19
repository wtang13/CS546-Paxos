/******************************************
	In this version, each node can die
******************************************/
# include "mpi.h"
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#define node_num 5
#define NO_VALUE_FOR_THIS_P 4
#define DATA_FOR_INIT_MEMO 101
#define FINISH_PROMISE 5
#define PROMISE_FOR_SAME_TASK 6
#define PROMISE_REJECT 7
#define FINISH_PROMISE_ACCEPT_ACCEPTED 0
#define PROPOSE_AGAIN 1
#define NODE_SAFE 2
#define NODE_DIE 3
#define MESSAGE_LOST 8

struct Proposal{
	int tag;
	char var;
	int value;
	int node_state
};

struct memo{
	char var;
	int data;
	int N;
	int node_state;
};
struct task{
	char var;
	int value;
};
int acceptor_state[node_num];
double promise_time[node_num];
int promise_state[node_num];
int index;
int task_num;
int mode;
struct Proposal p_queue[node_num];
/*********************************************************************
function: create task set for each node. In each node, the task is 
change according variable's value(write operation); init memo
example of task set: a3b2c5d7e19
*********************************************************************/
void Inittask_set(struct task task_set[],struct memo m[],int acceptor_state[],int myrank){
	char var_to_change[5]={'a','b','c','d','e'};
	int i,offset;
	for(i=0;i<task_num;i++){
		task_set[i].var=var_to_change[myrank];
		task_set[i].value=rand()%100;
	}
	for(i=0;i<node_num;i++){
		m[i].var='N';
		m[i].data=DATA_FOR_INIT_MEMO;
		m[i].N=-1;
		m[i].node_state=NODE_SAFE;
		acceptor_state[i]=1;
	}
}
/*********************************************************************
function: create monotonic increasing N for each proposal in each node
*********************************************************************/
int get_proposer_number(int taskth,int myrank){
	return node_num*taskth+myrank;
}

int get_node_state(int node_state){
	if(node_state==0){
		return NODE_SAFE;
	}
	if(node_state==1){
		return NODE_DIE;
	}
	if(node_state==2){
		return MESSAGE_LOST;
	}
}
/*********************************************************************
function: send it own proposal and node_state to all acceptors
*********************************************************************/
void Prepare(int myrank,struct Proposal p){
	int i=0;
   	//exchange tag
	int send_tag,send_value,source,send_node_state;
	char send_var;
	//source=0;
	for(source=0;source<node_num;source++){
		if(source==myrank){
			if(p.node_state==NODE_SAFE||p.node_state==NODE_DIE){
				send_tag=p.tag;
				send_value=p.value;
				send_var=p.var;
				send_node_state=p.node_state;
				p_queue[source].tag=send_tag;
				p_queue[source].var=send_var;
				p_queue[source].value=send_value;
				p_queue[source].node_state=send_node_state;
			}
			else{
				send_tag=-1;
				send_value=NO_VALUE_FOR_THIS_P;
				send_var='N';
				send_node_state=p.node_state;
				p_queue[source].tag=send_tag;
				p_queue[source].var=send_var;
				p_queue[source].value=send_value;
				p_queue[source].node_state=send_node_state;
			}

		}
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_value,1,MPI_INT,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_var,1,MPI_CHAR,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_tag,1,MPI_INT,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_node_state,1,MPI_INT,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		if(myrank !=source){
			p_queue[source].tag=send_tag;
			p_queue[source].var=send_var;
			p_queue[source].value=send_value;
			p_queue[source].node_state=send_node_state;
		}
	}
	//printf("finish Prepare in %d\n",myrank );
}

/*********************************************************************
function: check wether proposal's variable is in the memo or if the 
proposal is first promise(var in m will be 'N'). 
**********************************************************************/
char check_var(struct memo m[],char variable){
	int i,flag;
	flag=0;
	for(i=0;i<node_num;i++){
		if(m[i].var==variable)
			if(variable=='N')
				return 'W';
			else
				return variable;	
	}
	char var_to_change[5]={'a','b','c','d','e'};
	for(i=0;i<node_num;i++){
		if(var_to_change[i]==variable)
			return variable;
	}
	if(flag==0)
		return 'E';

}
/*********************************************************************
function: find the position of where this proposal might be update in 
memo
**********************************************************************/
int find_pos(struct memo m[],char variable){
	int i;
	for(i=0;i<node_num;i++)
		if(m[i].var==variable)
			return i;
}
/*********************************************************************
function: debug function
**********************************************************************/
void check_time(double promise_time[]){
	int i;
	for(i=0;i<node_num;i++)
		printf("promise time for rank %d is %lf\n",i,promise_time[i] );
}

void set_state(struct memo m[]){
	int i;
	for(i=0;i<node_num;i++){
		m[i].node_state=get_node_state(i%3);
	}

}

/*********************************************************************
function: promise proposals in p_queue or not;
if this proposal is first propose, promise
if p.tag>=N, promise
if p.tag<N, reject
if other proposal has been accepte,reject
**********************************************************************/
int Promise_stage(struct Proposal p,int myrank,struct memo m[]){

	//printf("start promise stage in %d\n",myrank );
	//1st: send proposal's variable,data to all acceptor
	srand((unsigned)time(NULL));
	double start,end,total=0;
	typedef struct{
		int N;
		char result;// Y or N
		int data;
	}acceptor_return_message;
	acceptor_return_message a_s_m[node_num],a_r_m[node_num];
	MPI_Datatype acceptor_return_message_type, oldtypes[2];// int and char
	int blockcounts[2];
	MPI_Aint    offsets[2], extent;
	MPI_Status Stat;
	//setup descriptyion of 1 MPI_CHAR fields for result
	offsets[0]=0;
	oldtypes[0]=MPI_CHAR;
	blockcounts[0]=1;
	// setup description of the 2 MPI_INT fields N, data
   // need to first figure offset by getting size of MPI_FLOAT
	MPI_Type_extent(MPI_CHAR, &extent);
   	offsets[1] = 1 * extent;
   	oldtypes[1] = MPI_INT;
   	blockcounts[1] = 2;
   	// define structured type and commit it
  	MPI_Type_struct(2, blockcounts, offsets, oldtypes, &acceptor_return_message_type);
   	MPI_Type_commit(&acceptor_return_message_type);
	//2. if rank != myrank, check on own memo
	int i,j=0,maxN=0,q,same_data=0;
	int promise_num=0,off=0;
	int tag1=1,flag=0;
	char reject='N';
	char promise='Y';
	char error_die='D';
	char error_lost='L';
   	
   	start=MPI_Wtime();
   	//each node process those proposal
   	for(i=0;i<node_num;i++){
   		a_r_m[i].N=0;
   		a_r_m[i].result='N';
   		a_r_m[i].data=0;
   		promise_time[i]=1.0;
   		promise_state[i]=0;
   	}

   	for(i=0;i<node_num;i++){
   		flag=0;
   		while(flag==0){
			if(acceptor_state[myrank]==0){//can't promise, reject to rank != my rank for there are mutilple proposal
				//send a blank message
					for(j=0;j<node_num;j++){
						a_s_m[j].result=reject;
						a_s_m[j].N= p_queue[myrank].tag;
						a_s_m[j].data=p_queue[myrank].value;
					
					}	
				flag=1;
			}else{
				if(p_queue[i].node_state==NODE_DIE){
					a_s_m[i].result=error_die;
					a_s_m[i].N= p_queue[myrank].tag;
					a_s_m[i].data=p_queue[myrank].value;
				}else{
					char check_var_result;
					check_var_result=check_var(m,p_queue[i].var);// process proposal from node i
					if(check_var_result=='W'){//node i lost message
							a_s_m[i].result=error_lost;
							a_s_m[i].N=p_queue[i].tag;
							a_s_m[i].data=p_queue[i].value;	
							
						}

					if(check_var_result=='N'){//no record for this variable, can promise
							a_s_m[i].result=promise;
							a_s_m[i].N= -1;
							a_s_m[i].data=DATA_FOR_INIT_MEMO;	
					}
					if(check_var_result==p_queue[i].var){
							j=find_pos(m,p_queue[i].var);
								if(m[j].N>p_queue[i].tag){//reject: N nor big enough
									a_s_m[i].result=reject;
									a_s_m[i].N= m[j].N;
									a_s_m[i].data=m[j].data;	
										
									}
								else{//legal to promise
									a_s_m[i].result=promise;
									a_s_m[i].N= m[j].N;
									a_s_m[i].data=m[j].data;	
									
								}
							}
						if(check_var_result=='E'){//receive wrong data
							a_s_m[i].result=reject;
							a_s_m[i].N=-1;
							a_s_m[i].data=DATA_FOR_INIT_MEMO;	
							
						}
				
				flag=1;	
			}
   		}
	}
}
	MPI_Barrier(MPI_COMM_WORLD);
	//gather from a_s_m[myrank] from each node
	for(i=0;i<node_num;i++){
		MPI_Gather(&a_s_m[i], 1, acceptor_return_message_type, a_r_m, 1, acceptor_return_message_type, i,MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
	}
	//printf("finish communicate\n");
	total=start-end;
	promise_time[myrank]=total;
	//printf("send_time in %d rank is %lf\n",myrank,promise_time[myrank] );
	int count=node_num-1;
	for(i=0;i<node_num;i++){
		if(a_r_m[i].result==promise)
			promise_num=promise_num+1;
		if(a_r_m[i].result==error_die)
			count=count-1;
	}
	if(promise_num>=((int)(count+1)/2))
		promise_state[myrank]=1;
	else
		promise_state[myrank]=0;
	end=MPI_Wtime();
	if(myrank==0)
		printf("all done\n");
	double send_time;
	promise_time[myrank]=end-start;
	int send_state,source;
	for(source=0;source<node_num;source++){
		if(source==myrank){// change elpased time
			send_time=promise_time[myrank];
			send_state=promise_state[myrank];
		}
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_time,1,MPI_DOUBLE,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Bcast(&send_state,1,MPI_INT,source, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		if(myrank !=source){
			promise_time[source]=(double)send_time;
			promise_state[source]=send_state;
		}
	}
		double min=LLONG_MAX;
		index=node_num;
		for(i=0;i<node_num;i++){
			if(promise_state[i]==1){
				if(min>=promise_time[i]){
					min=promise_time[i];
					index=i;
				}
			}
		}
		//printf("index is %d\n", index);
		if(index==node_num){//all node die or lost message
			printf("all nodes dead\n");
			MPI_Finalize();
			exit(0);

		}
	//all node should reject to all proposal now, for node index has become the master node
	for(i=0;i<node_num;i++)
		acceptor_state[i]=0;
	MPI_Type_free(&acceptor_return_message_type);
	if(myrank==index){
		//printf("fininsh promise %d\n",myrank );
		return FINISH_PROMISE;
	}
	else{
		//printf("fininsh promise %d\n",myrank );
		return PROMISE_REJECT;
	}

}
/*********************************************************************
function: do promise, accept and accepted stage job;
if a proposal from myrank node is accepted, other node will update their
memo according to thie proposal.
**********************************************************************/
int Promise_Accept_Accepted(struct Proposal p, int myrank,struct memo m[],int acceptor_state[]){
	//compare N with N in memo
	//we need m.var to find N

	char var_to_change[5]={'a','b','c','d','e'};
	MPI_Status Stat; 
	int promise_return_code;
	int i,send_data,send_N,send_rank;
	promise_return_code=Promise_stage(p,myrank,m);
	if(myrank==index)
		printf("select %d node as master node\n",myrank);
	//accept stage
	//add to according index for memo
	//printf("in accept stage,  %d\n",myrank);
	if(m[index].data==DATA_FOR_INIT_MEMO||p_queue[index].value>=m[index].data){
		if(m[index].data==DATA_FOR_INIT_MEMO){
			m[index].var=var_to_change[index];
		}
		m[index].data=p_queue[index].value;
		m[index].N=p_queue[index].tag;
	}
	//finish accept
	for(i=0;i<node_num;i++)
		acceptor_state[i]=1;
	//printf("finish accept,  %d\n",myrank);
	if(promise_return_code==FINISH_PROMISE){
		//printf("accept stage end\n");
		return FINISH_PROMISE_ACCEPT_ACCEPTED;
		}
	if(promise_return_code==PROMISE_REJECT){
		//printf("accept stage end\n");
		return PROPOSE_AGAIN;
	}
}
/*********************************************************************
function: debug function
**********************************************************************/
void check_memo(struct memo m[]){
	int i;
	printf("Now data in memo is : in N, VAR, DATA state order\n");
	for(i=0;i<node_num;i++){
		printf(" %d %c %d %d\n",m[i].N,m[i].var,m[i].data,m[i].node_state);
	}
}
/*********************************************************************
function: init P_queue for each round of select master
**********************************************************************/
void cleanP_queue(struct Proposal p[]){
	int i;
	for(i=0;i<node_num;i++){
		p[i].tag=-1;
		p[i].var='N';
		p[i].value=NO_VALUE_FOR_THIS_P;
	}
}
int get_round(struct memo m[],int myrank){
	int c=0,i,count;
	count=1;
		for(i=0;i<node_num;i++){
			if(m[i].node_state==NODE_SAFE)
				c=c+1;
			else{
				if(myrank==0)
					printf("%d is dead or lost message\n", i);
			}
		}
		count=c*task_num; 
	return count;

}
int parse_args(int argc, char **argv){
	if(argc==3){
		task_num=atoi(argv[1]);
		mode=atoi(argv[2]);
	}else{
		printf("Usage: %s <task num for each node> [mode: 0 for all safe 1 for error case] \n",
           argv[0]);    
    	exit(0);
	}
}

/*********************************************************************
function: do selected master node's work
**********************************************************************/
int main(int argc,char**argv){
	parse_args(argc, argv);
	struct task task_set[task_num];
	struct memo m[node_num];
	int myrank,numprocessor;
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocessor);
	Inittask_set(task_set,m,acceptor_state,myrank);
	int j,i;
	int taskth=1;
	i=0;
	int offset=0;
	if(myrank==0)
			check_memo(m);
	int r=0;
	if(mode==1){
		set_state(m);
		r=get_round(m,myrank);
	}else
		r=get_round(m,myrank);
	for(j=0;j<r;j++){
			cleanP_queue(p_queue);
			int returncode;
			struct Proposal p;
			if(i<task_num){
				if(m[myrank].node_state==NODE_SAFE){
					p.var=task_set[i].var;
					p.value=task_set[i].value;
					p.tag=get_proposer_number(taskth,myrank);
					p.node_state=m[myrank].node_state;
					p_queue[myrank]=p;
					Prepare(myrank,p);
					returncode=Promise_Accept_Accepted(p_queue[myrank],myrank,m,acceptor_state);
					if(returncode==FINISH_PROMISE_ACCEPT_ACCEPTED)
						i=i+1;
					else
						taskth=taskth+1;

				}
				if(m[myrank].node_state==NODE_DIE||m[myrank].node_state==MESSAGE_LOST){
					p.var='N';
					p.value=NO_VALUE_FOR_THIS_P;
					p.tag=get_proposer_number(taskth,myrank);
					p_queue[myrank]=p;
					p.node_state=MESSAGE_LOST;
					Prepare(myrank,p);
					Promise_Accept_Accepted(p_queue[myrank],myrank,m,acceptor_state);
				}	
			}else{// no right to propose,send blank info
				p.var='N';
				p.value=NO_VALUE_FOR_THIS_P;
				p.tag=get_proposer_number(taskth,myrank);
				p_queue[myrank]=p;
				p.node_state=MESSAGE_LOST;
				Prepare(myrank,p);
				Promise_Accept_Accepted(p_queue[myrank],myrank,m,acceptor_state);

			}
			
		

	}
	MPI_Barrier(MPI_COMM_WORLD);
	printf("conference end, check memo in node %d\n",myrank);
	check_memo(m);
	//free unnecessary resource

	MPI_Finalize();

	exit(0);		

}
