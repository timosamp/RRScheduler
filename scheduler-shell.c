#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 5               /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */


/* struct for the proccess list*/
struct proc_node
{
	pid_t pid;
	struct proc_node *next;
	int id;
	char priority;
};

typedef struct proc_node proc_node;
/* Global variables */
 proc_node *temp_node, *head, *tail, *pos;
 int i;
 int counter_h_prior = 0;

/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
	temp_node = head;
	while(temp_node != NULL)
	{
		printf("pid: %d id: %d with priority %c\n", temp_node->pid, temp_node->id, temp_node->priority);
		temp_node = temp_node->next;
	}
	//printf("pid_head: %ld, pid_tail: %ld\n", head->pid, tail->pid);
	printf("\nproccess with PID: %d and id: %d is running now\n\n", head->pid, head->id);
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id)
{
	proc_node *temp, *prev = NULL;
	pid_t p;
	int status;

	temp = head;

	if(id > i)
	{
		printf("\nThere is not this id\n" );
		return 0;
	}

	if(id == 0)
	{
		printf("Bad command, shell cant kill itshelf\n");
		return 0;
	}
	while(temp != NULL)
	{
		if(id == temp->id)
		{
			if(prev != NULL)	//einai endiamesos komvos
			{
				prev->next = temp->next;

				if(temp->next == NULL)
				{
					tail = prev;
				}
			}else{						// allios einai to head kai den exei prev_node
				head = head->next;
			}

			kill(temp->pid, SIGKILL);
			free(temp);
			break;
		}
		//printf("pid: %ld\n", temp_node->pid);
		prev = temp;
		temp = temp->next;
	}

	for(;;)
	{
		//wait for the stopped or dead child
		p = waitpid(-1, &status, WNOHANG | WUNTRACED);
		if (p < 0) {
			perror("waitpid");
			exit(1);
		}
		if(p == 0)
		{
			break;
		}
	}

	//assert(0 && "Please fill me!");
	return -ENOSYS;
}


/* Create a new task.  */
static void
sched_create_task(char *executable)
{
	pid_t p, pid;
	int status;
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };

	pid = fork();
	if (pid < 0) {
		perror("scheduler: fork");
		exit(1);
	}

	if (pid == 0) {

		raise(SIGSTOP);		//raise SIGSTOP before exec()ing
		execve(executable, newargv, newenviron);
		perror("execve");
		exit(1);
	}

	//find the first correct position
	temp_node = head;
	while((temp_node->next != NULL) && ((temp_node->next->priority != 'l')||(temp_node->next->id == 0)))
	{
		temp_node = temp_node->next;
	}

	//construct the node
	pos = (proc_node *)malloc(sizeof(proc_node));

	//connections for the list
	pos->next = temp_node->next;
	temp_node->next = pos;

	//fill the attributes
	pos->pid = pid;
	pos->id = i;
	pos->priority = 'l';

	//increase the i for the next node's id
	i++;

	//if the struct inserted at the end of the list change the tail's pointer
	if(pos->next == NULL)
	{
		tail = pos;
	}
	printf("The head then had the id: %d\n", head->id);
	printf("id: %d pid: %d\n", pos->id, pos->pid);

	for(;;)
	{
		//wait for the stopped or dead child
		p = waitpid(pid, &status, WNOHANG | WUNTRACED);
		if (p < 0) {
			perror("waitpid");
			exit(1);
		}
		if(p == 0)
		{
			break;
		}
	}
}

static void sched_change_priority(char state, int id)
{

	temp_node = head;
	while(temp_node != NULL)
	{
		if(temp_node->id == id)
		{
			if(temp_node->priority != state)
			{
				printf("\nProccess with id %d, change priority from %c to %c\n\n", id, temp_node->priority, state);
				temp_node->priority = state;

				if(state == 'l')
				{
					counter_h_prior--;
				}else{
					counter_h_prior++;
				}

			}else{
				printf("\nProccess with id %d, has already priority %c\n\n", id, temp_node->priority);
			}
			return;
		}
		temp_node = temp_node->next;
	}
	printf("\nThere is not this id\n\n" );
}

/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq)
{
	switch (rq->request_no) {
		case REQ_PRINT_TASKS:
			sched_print_tasks();
			return 0;

		case REQ_KILL_TASK:
			return sched_kill_task_by_id(rq->task_arg);

		case REQ_EXEC_TASK:
			sched_create_task(rq->exec_task_arg);
			return 0;
		case REQ_HIGH_TASK:
			sched_change_priority('h',rq->task_arg);
			return 0;
		case REQ_LOW_TASK:
			sched_change_priority('l',rq->task_arg);
			return 0;

		default:
			return -ENOSYS;
	}
}

/*
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
	/* apenergopoioume ta simata kata tin ektelesi tou handler */
	//signals_disable();
	kill(head->pid, SIGSTOP);

	// if(head->id == 0)
	// {
	// 	signals_disable();
	// }
	//signals_enable();
}

/*
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum)
{
	pid_t p;
	int status;

	/* apenergopoioume ta simata kata tin ektelesi tou handler */
	//signals_disable();

	/* Error */
	if (signum != SIGCHLD) {
		fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n",
			signum);
		exit(1);
	}

	for(;;)
	{
		//wait for the stopped or dead child
		p = waitpid(-1, &status, WNOHANG | WUNTRACED);
		if (p < 0) {
			perror("waitpid");
			exit(1);
		}
		if(p == 0)
		{
			break;
		}


		//check if the child stopped or died
		if(WIFEXITED(status))
		{
			fprintf(stderr, "Child PID = %ld terminated normally\n", (long)head->pid);

			if(head->priority == 'h')
			{
				counter_h_prior--;
			}

			temp_node = head;
			head = head->next;
			free(temp_node);

			/*if list is empty stop the main program*/
			//printf("head: %d\n", head);
			if(head == NULL) {
				exit(0);
			}

			while(((counter_h_prior > 0)&&(head->priority != 'h')) && (head->id != 0))
			{
				if(head->next != NULL)
				{
					temp_node = head;
					head = head->next;
					temp_node->next = NULL;
					tail->next = temp_node;
					tail = tail->next;
				}
			}

		}

		if(WIFSTOPPED(status))
		{
			fprintf(stderr, "Child PID = %ld stopped normally\n", (long)head->pid);

			/*if list has only one proccess then continue with this else ...*/
			do {
				if(head->next != NULL)
				{
					temp_node = head;
					head = head->next;
					temp_node->next = NULL;
					tail->next = temp_node;
					tail = tail->next;
				}
			} while(((counter_h_prior > 0)&&(head->priority != 'h')) && (head->id != 0));

		}

		if(WIFSTOPPED(status) || WIFEXITED(status))
		{
			/* Setup the alarm again */
			if (alarm(SCHED_TQ_SEC) < 0) {
				perror("alarm");
				exit(1);
			}
			/*continue with the next proccess*/
			kill(head->pid, SIGCONT);
		}

	}
}

/* Disable delivery of SIGALRM and SIGCHLD. */
static void signals_disable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	//an ta energopoiiso tha treksoun ola siriaka!!
	sigaddset(&sigset, SIGCHLD);	//gia na aporeiptontai auta ta simata
	sigaddset(&sigset, SIGALRM);	//otan ekteleitai o handler
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void signals_enable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
}


/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	// sigaddset(&sigset, SIGCHLD);
	// sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}

	/*
	 * Ignore SIGPIPE, so that write()s to pipes
	 * with no reader do not result in us being killed,
	 * and write() returns EPIPE instead.
	 */
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}

static void
do_shell(char *executable, int wfd, int rfd)
{
	char arg1[10], arg2[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };

	sprintf(arg1, "%05d", wfd);
	sprintf(arg2, "%05d", rfd);
	newargv[1] = arg1;
	newargv[2] = arg2;

	raise(SIGSTOP);		//sikonoume SIGSTOP opos kai sti alles efarmoges.
	execve(executable, newargv, newenviron);	//arxizoume tin ektelesi opote
																						//mas steilei o sched SIGCONT
	/* execve() only returns on error */
	perror("scheduler: child: execve");
	exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
//static void
pid_t sched_create_shell(char *executable, int *request_fd, int *return_fd)
{
	pid_t p;
	int pfds_rq[2], pfds_ret[2];

	if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
		perror("pipe");
		exit(1);
	}

	p = fork();
	if (p < 0) {
		perror("scheduler: fork");
		exit(1);
	}

	if (p == 0) {
		/* Child */
		// sto pfds_rq[1] grafei to paidi, diavazei o scheduler sto pfds_rq[0]
		close(pfds_rq[0]);
		// sto pfds_ret[1] grafei o scheduler, diavazei to paidi sto pfds_ret[0]
		close(pfds_ret[1]);
		do_shell(executable, pfds_rq[1], pfds_ret[0]); //mesa edw tha sikothei
		assert(0);																		 //SIGSTOP
	}
	/* Parent */
	close(pfds_rq[1]);
	close(pfds_ret[0]);
	//printf("\n %d %d \n", *request_fd, *return_fd);
	*request_fd = pfds_rq[0];
	*return_fd = pfds_ret[1];
	//printf("\n %d %d \n", *request_fd, *return_fd);
	return p;
}

static void
shell_request_loop(int request_fd, int return_fd)
{
	int ret;
	struct request_struct rq;

	/*
	 * Keep receiving requests from the shell.
	 */

	/*test for the fds:
	printf("\n %d %d \n", request_fd, return_fd);*/

	for (;;) {
		if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
			perror("scheduler: read from shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}


		signals_disable();
		ret = process_request(&rq);
		signals_enable();


		if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
			perror("scheduler: write to shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
		//printf("\nedwwwwwwwwwwwwwwwwwwwwwwwwwwwwww3\n\n");
	}
}

int main(int argc, char *argv[])
{
	int nproc;
	pid_t pid, shell_pid;
	/* Two file descriptors for communication with the shell */
	static int request_fd, return_fd;

	/* Create the shell. Return the shell's pid */
	shell_pid = sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);

	/* Add the shell to the scheduler's tasks (list) */
	head = (proc_node *)malloc(sizeof(proc_node));
	head->pid = shell_pid;
	head->id = 0;
	head->next = NULL;
	head->priority = 'l';
	tail = head;
	//printf("Shell id: 0\n");

	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */

	char *executable; //= "prog";
  char *newargv[] = { executable, NULL, NULL, NULL };
  char *newenviron[] = { NULL };

 	nproc = argc-1; /* number of proccesses goes here */

 	if (nproc == 0) {
 		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
 		exit(1);
 	}

 	for(i = 1; i <= nproc; i++)
 	{
 		//executable = (char *) malloc(sizeof(*argv[i]))
 		executable = argv[i];	//gia diaforetika orismata
 		newargv[0] = executable;
		//printf("       %s       \n", executable);
 		pid = fork();

 		if (pid < 0) {
 			/* Error */
 			perror("prog");
 			exit(1);
 		}
 		if (pid == 0) {
 			/* Child */
 			raise(SIGSTOP);		//raise SIGSTOP before exec()ing
 			execve(executable, newargv, newenviron);
 			perror("execve");
 			exit(1);
 		}

 		/*construct the proccess list,
 		add nodes at the end of the list*/

		temp_node = (proc_node *)malloc(sizeof(proc_node));
		tail->next = temp_node;
		tail = temp_node;
		tail->pid = pid;
		tail->next = NULL;
		tail->id = i;
		tail->priority = 'l';
		//printf("pid: %ld, id: %d\n", pid, i);

 	}
	//printf("\n%d\n\n", counter_h_prior);

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc + 1);

	//test for list's connectivity
	temp_node = head;
	while(temp_node != NULL)
	{
		printf("pid: %d, id: %d\n", temp_node->pid, temp_node->id);
		temp_node = temp_node->next;
	}
	printf("pid_head: %d, pid_tail: %d\n", head->pid, tail->pid);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	/*Set up the timmer to raise SIGALRM*/
	if (alarm(SCHED_TQ_SEC) < 0) {
		perror("alarm");
		exit(1);
	}

	/*start running the first proccess*/
	kill(head->pid, SIGCONT);


	//loop in the shell read-execute commands

	shell_request_loop(request_fd, return_fd);


	/* Now that the shell is gone, just loop forever
	 * until we exit from inside a signal handler.
	 */
	while (pause())
		;

	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}
