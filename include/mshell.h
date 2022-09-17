/*
	Signal handlers
*/
void setSigchldHandler();
void setSigintHandler();
void handleSigchld(int);
void handleSigint(int);

/*
	Wrapper functions
*/
void dup2AndClose(int, int);
int forkOrExit();
void pipeOrExit(int*);
void closeOrReportError(int);
void writeLine(int, char*);

/*
	Error handling functions
*/
void dealWithErrors();
void dealWithRedirectionErrors();

/*
	Input parsing functions
*/
bool substituteFirstEndline();
void removeFirstLine();
int checkCommand(char*);
bool backgroundProcess(int);
void fillPipelineLength();

/*
	Command executing functions
*/
int executeBuiltinCommand(char*, char**);
void executeCommand(bool);

/*
	Helper functions
*/
void writeDownTerminatedChld();
void dealWithRedirections();
void waitForForegroundProcess();
void readLine(bool);
void handleCommands();
void readFromFile();
void readFromTerminal();

