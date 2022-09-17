#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"
#include "input_parse/siparseutils.h"
#include "mshell.h"
/*
	Variables
*/
line* ln;
command* com;
pipeline* pipelines;
sigset_t mask;

struct stat statBuf;
struct sigaction sigchlda;
struct sigaction siginta;

char buf[MAX_LINE_LENGTH + 1];

bool tooLongLine = false;
bool endlinePresent;

int n = 0;
int pipelineLength;
int chldProcessesInd = 0;
int foregroundProcess = -1;
int chldProcessesQueue[512][2];
/*
	Signal handlers
*/
void setSigchldHandler()
{
	sigchlda.sa_handler = handleSigchld;
	sigemptyset(&sigchlda.sa_mask);
	sigchlda.sa_flags = 0;
	sigaction(SIGCHLD, &sigchlda, NULL);
}

void setSigintHandler()
{
	siginta.sa_handler = handleSigint;
	sigemptyset(&siginta.sa_mask);
	siginta.sa_flags = 0;
	sigaction(SIGINT, &siginta, NULL);
}

void handleSigchld(int signal)
{
	pid_t pid = 1;
	int status;
	while (pid > 0)
	{
		pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0)
		{	
			if (pid != foregroundProcess)
			{
				chldProcessesQueue[chldProcessesInd][0] = pid;
				chldProcessesQueue[chldProcessesInd][1] = status;
				chldProcessesInd++;
			}
			else
				foregroundProcess = -1;
		}
	}
}

void handleSigint(int signal)
{
	
}

/*
	Wrapper functions
*/
void dup2AndClose(int oldfd, int newfd)
{
	dup2(oldfd, newfd);
	if (newfd == STDIN_FILENO)
	{
		if (oldfd != STDIN_FILENO)
			closeOrReportError(oldfd);
	}
	if (newfd == STDOUT_FILENO)
	{
		if (oldfd != STDOUT_FILENO)
			closeOrReportError(oldfd);
	}
}

int forkOrExit()
{
	int k = fork();
	if (k < 0)
	{
		perror("Fork failure");
		exit(EXEC_FAILURE);
	}
	return k;
}

void pipeOrExit(int* fd)
{
	if (pipe(fd) == -1)
	{
		perror("Pipe failure");
		exit(EXEC_FAILURE);
	}
}

void closeOrReportError(int fd)
{
	if (close(fd) == -1)
		perror("Close failure");
}

void writeLine(int desc, char* str)
{
	int charsToWrite = strlen(str);
	int charsWritten = 0;
	while (charsWritten < charsToWrite)
	{
		int numberOfWrittenChars = write(desc, str + charsWritten, strlen(str + charsWritten));
		if (numberOfWrittenChars == -1)
		{
			perror("Write failure");
			exit(EXEC_FAILURE);
		}
		charsWritten += numberOfWrittenChars;
	}
}

/*
	Error handling functions
*/
void dealWithErrors()
{
	if (errno)
	{
		if (errno == ENOENT)
			writeLine(STDERR_FILENO, strcat(com->argv[0], ": no such file or directory\n"));
		else
		{
			if (errno == EACCES)
				writeLine(STDERR_FILENO, strcat(com->argv[0], ": permission denied\n"));
			else
				writeLine(STDERR_FILENO, strcat(com->argv[0], ": exec error\n"));
		}
		exit(EXEC_FAILURE);
	}
}

void dealWithRedirectionErrors()
{
	if (errno)
	{
		if (errno == ENOENT)
			writeLine(STDERR_FILENO, strcat(com->redirs[0]->filename, ": no such file or directory\n"));
		else
		{
			if (errno == EACCES)
				writeLine(STDERR_FILENO, strcat(com->redirs[0]->filename, ": permission denied\n"));
		}
		exit(EXEC_FAILURE);
	}
}

/*
	Input parsing functions
*/
bool substituteFirstEndline()
{
	int i;
	bool substituted = false;
	for (i = 0; i < n; i++)
	{
		if (buf[i] == '\n')
		{
			buf[i] = 0;
			substituted = true;
			break;
		}
	}
	buf[n] = 0;
	return substituted;
}

void removeFirstLine()
{
	char temporaryBuf[MAX_LINE_LENGTH + 1];
	int i;
	int indexInBuf = 0;
	for (i = 0; i < n; i++)
		temporaryBuf[i] = buf[i];
	i = 0;
	while (i < n && temporaryBuf[i])
		i++;
	i++;
	for (i; i < n; i++)
		buf[indexInBuf++] = temporaryBuf[i];
	n = indexInBuf;
}

int checkCommand(char* command)
{
	int i = 0;
	while (builtins_table[i].name != NULL)
	{
		if (!strcmp(command, builtins_table[i].name))
			return false;
		i++;
	}
	return true;
}

bool backgroundProcess(int background)
{
	if (background == LINBACKGROUND)
		return true;
	return false;
}

void fillPipelineLength()
{
	command** p = (*pipelines);
	pipelineLength = 0;
	command* temp = com;	
	while ((*p) != NULL)
	{
		pipelineLength++;
		p++;
	}	
	pipelines++;
}

/*
	Command executing functions
*/
int executeBuiltinCommand(char* cmd, char** arguments)
{
	int i = 0;
	while (builtins_table[i].name != NULL)
	{
		if (!strcmp(cmd, builtins_table[i].name))
			return builtins_table[i].fun(arguments);
		i++;
	}
}

void executeCommands(bool background)
{
	pipelines = ln->pipelines;
	com = pickfirstcommand(ln);
	while (*pipelines)
	{
		fillPipelineLength();
		if (checkCommand(com->argv[0]))
		{
			int fd[2];
			int i = 0;
			int in = STDIN_FILENO;
			int foregroundProcessesCount = 0;
			int pid;
			for (i = 0; i < pipelineLength - 1; i++, com++)
			{
				pipeOrExit(fd);
				pid = forkOrExit();
				if (!pid)
				{
					if (background)
						setsid();
					closeOrReportError(fd[0]);
					dup2AndClose(in, STDIN_FILENO);
					dup2AndClose(fd[1], STDOUT_FILENO);
					if (com->redirs[0] != NULL) // redirection; redirections are handled first
						dealWithRedirections();
					if (execvp(com->argv[0], com->argv) < 0)
						dealWithErrors();
				}
				else
				{
					closeOrReportError(fd[1]);
					if (in != STDIN_FILENO)
						closeOrReportError(in);
					in = fd[0];
					if (!background)
					{
						foregroundProcess = pid;
						waitForForegroundProcess();
					}
				}
			}
			
			pid = forkOrExit();
			if (!pid)
			{
				if (background)
					setsid();
				dup2AndClose(in, STDIN_FILENO);
				if (com->redirs[0] != NULL)
					dealWithRedirections();
				if (execvp(com->argv[0], com->argv) < 0)
					dealWithErrors();
			}
			else
			{
				if (in != STDIN_FILENO)
					closeOrReportError(in);
				if (!background)
				{
					foregroundProcess = pid;
					waitForForegroundProcess();
				}
			}
		}
		else
		{
			if (executeBuiltinCommand(com->argv[0], com->argv))
			{
				char errorMessage[MAX_LINE_LENGTH];
				strcpy(errorMessage, "Builtin ");
				strcat(errorMessage, com->argv[0]);
				strcat(errorMessage, " error.\n");
				writeLine(STDERR_FILENO, errorMessage);
			}
			break;
		}
		com++;
	}
}

/*
	Helper functions
*/
void writeDownTerminatedChld()
{
	int i;
	for (i = 0; i < chldProcessesInd; i++)
	{
		if (!chldProcessesQueue[i][1])
			printf("Background process (%d) terminated. (exited with status(%d))\n", chldProcessesQueue[i][0], chldProcessesQueue[i][1]);
		else
			printf("Background process (%d) terminated. (killed by signal(%d))\n", chldProcessesQueue[i][0], chldProcessesQueue[i][1]);
	}
	chldProcessesInd = 0;
}

void dealWithRedirections()
{
	int fd;
	if (IS_RIN(com->redirs[0]->flags)) // input from a file
	{
		fd = open(com->redirs[0]->filename, O_RDONLY, S_IWUSR | S_IRUSR);
		if (fd == -1)
			dealWithRedirectionErrors();
		dup2AndClose(fd, STDIN_FILENO);
	}
	if (IS_ROUT(com->redirs[0]->flags) || (com->redirs[1] != NULL && IS_ROUT(com->redirs[1]->flags)))
	{
		if (IS_ROUT(com->redirs[0]->flags))
			fd = open(com->redirs[0]->filename, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		else
			fd = open(com->redirs[1]->filename, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		if (fd == -1)
			dealWithRedirectionErrors();
		dup2AndClose(fd, STDOUT_FILENO);
	}
	if (IS_RAPPEND(com->redirs[0]->flags) || (com->redirs[1] != NULL && IS_RAPPEND(com->redirs[1]->flags)))
	{
		if (IS_RAPPEND(com->redirs[0]->flags))
			fd = open(com->redirs[0]->filename, O_APPEND | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		else
			fd = open(com->redirs[1]->filename, O_APPEND | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		if (fd == -1)
			dealWithRedirectionErrors();
		dup2AndClose(fd, STDOUT_FILENO);
	}
}

void waitForForegroundProcess()
{
	sigset_t mask, oldmask;
	sigdelset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	while (foregroundProcess != -1)
		sigsuspend(&oldmask);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void readLine(bool terminal)
{
	int numberOfReadChars;
	bool toRead = true;
	while (n < MAX_LINE_LENGTH && toRead)
	{
		if (!terminal)
		{
			if (n && buf[n - 1] == EOF)
				toRead = false;
		}
		else
		{
			if (n && buf[n - 1] == '\n')
				toRead = false;
		}
		if (toRead)
		{
			numberOfReadChars = read(STDIN_FILENO, buf + n, MAX_LINE_LENGTH - n);
			if (!numberOfReadChars)
				toRead = false;
			if (numberOfReadChars > 0)
				n += numberOfReadChars;
		}
	}
}

bool dealWithTooLongLine()
{
	if (!endlinePresent)
	{
		if (!tooLongLine)
			writeLine(STDERR_FILENO, "Syntax error.\n");
		tooLongLine = true;
		removeFirstLine();
		return true;
	}
	if (endlinePresent && tooLongLine)
	{
		tooLongLine = false;
		removeFirstLine();
		return true;
	}
	return false;
}

void handleCommands()
{
	endlinePresent = substituteFirstEndline();
	if (dealWithTooLongLine())
		return;
	ln = parseline(buf);
	com = pickfirstcommand(ln);
	if (com->argv[0] == NULL)
	{
		removeFirstLine();
		return;
	}
	executeCommands(backgroundProcess(ln->flags));
	removeFirstLine();
}

void readFromFile()
{
	while (1)
	{
		readLine(false);
		if (n <= 0)
			break;
		handleCommands();
	}
}

void readFromTerminal()
{
	while (1)
	{
		writeDownTerminatedChld();
		write(STDOUT_FILENO, PROMPT_STR, strlen(PROMPT_STR));
		readLine(true);
		handleCommands();
	}
}

/*

*/
int main(int argc, char *argv[])
{
	setSigchldHandler();
	setSigintHandler();
	fstat(0, &statBuf);
	
	if (!S_ISCHR(statBuf.st_mode))
		readFromFile();
	else
		readFromTerminal();
}

