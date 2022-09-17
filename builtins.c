#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>

#include "builtins.h"

int lecho(char* []);
int lls(char* []);
int lcd(char* []);
int lkill(char* []);
int lexit(char* []);

builtin_pair builtins_table[]={
	{"exit",	&lexit},
	{"lecho",	&lecho},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,		NULL}
};

int lecho(char* argv[])
{
	int i = 1;
	if (argv[i]) printf("%s", argv[i++]);
	while (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int lls(char* argv[])
{
	DIR* directory;
	struct dirent* file;
	char cwd[1024];
	getcwd(cwd, sizeof(cwd));
	directory = opendir(cwd);
	while (1)
	{
		file = readdir(directory);
		if (file == NULL)
			break;
		if (file->d_name[0] != '.')
			printf("%s\n", file->d_name);
	}
	fflush(stdout);
	closedir(directory);
	return 0;
}

int lexit(char* argv[])
{
	_exit(0); // ignoring arguments
}

int lcd(char* argv[])
{
	if (argv[1] != NULL && argv[2] != NULL)
		return 1;
	if (argv[1] == NULL)
		return chdir(getenv("HOME"));
	else
		return chdir(argv[1]);
}

int lkill(char* argv[])
{
	if (argv[1] == NULL)
		return -1;
	char signal[10];
	if (argv[2] != NULL)
	{
		int i = 1;
		while (argv[1][i])
		{
			signal[i - 1] = argv[1][i];
			i++;
		}
		signal[i - 1] = 0;
		if (!atoi(argv[2]) || !atoi(signal))
			return -1;
		return kill(atoi(argv[2]), atoi(signal));
	}
	else
	{
		if (!atoi(argv[1]))
			return -1;
		return kill(atoi(argv[1]), SIGTERM);
	}
}

