#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <fcntl.h>
#include <sys/wait.h>
#include <algorithm>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }

    return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

static void getCurrentDirectoryName(char* buffer) {
    SmallShell& shell = SmallShell::getInstance();
    getcwd(buffer, FILENAME_MAX);
}

Command::Command(const char *cmd_line) {
    this->cmd_line = new char[strlen(cmd_line) + 1];
    strcpy(this->cmd_line, cmd_line);
    arguments_number = _parseCommandLine(cmd_line, args);
}

Command &Command::operator=(const Command &command) {
    if (this == &command) {
        return *this;
    }

    cmd_line = new char[strlen(command.cmd_line) + 1];
    strcpy(this->cmd_line, command.cmd_line);
    arguments_number = _parseCommandLine(cmd_line, args);

    return *this;
}

Command::~Command() {
    delete[] cmd_line;
}

void ChangePromptCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    // there are no arguments
    if (this->getArgByIndex(1) == nullptr) {
        shell.setCurrentPrompt("smash");
    }
    else {
        shell.setCurrentPrompt(this->getArgByIndex(1));
    }
}

void ShowPidCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash pid is " << shell.getSmashPID() << std::endl;
}


void GetCurrDirCommand::execute() {
    char buffer[FILENAME_MAX];
    getCurrentDirectoryName(buffer);
    std::cout << buffer << std::endl;
}

void ChangeDirCommand::execute() {
    if (this->getArgByIndex(2) != nullptr) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }

    // there is '-' in the cd command

    if (strcmp(this->getArgByIndex(1), "-") == 0) {
        // the last working directory was empty
        if(*(this->last_directory_path) == nullptr) {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }
        //set the current directory to the last one

        getCurrentDirectoryName(current_directory);

        //printf("current in -1: %s\n", current_directory);
        //printf("last in -1: %s\n", *last_directory_path);
        if (chdir(*(this->last_directory_path)) == -1) {
            perror("smash error: chdir failed");
            return;
        }

        *last_directory_path = current_directory;
        //printf("last in -2: %s\n", *last_directory_path);
        return;
    }
    else {
        getCurrentDirectoryName(current_directory);
        if (chdir(this->getArgByIndex(1)) == -1) {
            perror("smash error: chdir failed");
            return;
        }
        *last_directory_path = current_directory;
        //strcpy(*last_directory_path, current_directory);
        //printf("last in else: %s\n", *last_directory_path);
        return;
    }
}

JobsList::JobEntry &JobsList::JobEntry::operator=(const JobsList::JobEntry &job_entry) {
    if (this == &job_entry) {
        return *this;
    }

    job_id = job_entry.job_id;
    pid = job_entry.pid;
    elapsed_time = job_entry.elapsed_time;
    is_stopped = job_entry.is_stopped;
    command = job_entry.command;

    return *this;
}


void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    JobEntry* job = new JobEntry(getMaxJobID() + 1, pid, isStopped, cmd);
    jobs.push_back(job);
}

void JobsList::printJobsList() {
    for (JobEntry* job:jobs) {
        if (job->is_stopped) {
            std::cout << "[" << job->job_id << "] " << job->command->getCmdLine() << " : " << job->pid << " " <<
            (int) difftime(time(nullptr), job->elapsed_time) << " secs (stopped)" << std::endl;
        }
        else {
            std::cout << "[" << job->job_id << "] " << job->command->getCmdLine() << " : " << job->pid << " " <<
                      (int) difftime(time(nullptr), job->elapsed_time) << " secs" << std::endl;
        }
    }
}

void JobsList::killAllJobs() {
    removeFinishedJobs();
    for (JobEntry* job:jobs) {
        if (kill(job->pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
    }
    if (!jobs.empty()) {
        jobs.clear();
    }
    removeFinishedJobs();
}

void JobsList::removeFinishedJobs() {
    int index = 0;
    for (JobEntry* job:jobs) {
        int waitResult = waitpid(job->pid, nullptr, WNOHANG);
        if (waitResult != 0) {
            jobs.erase(jobs.begin() + index);
        }
        index++;
    }
}

void JobsList::finishedJobs() {
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid_smash = getpid();

    if(pid_smash < 0) {
        std::perror("smash error: getpid failed");
        return;
    }

    if(pid_smash != shell.getSmashPID()){
        return;
    }

    for(unsigned int i = 0; i < jobs.size(); i++) {
        JobsList::JobEntry* job = jobs[i];
        int waitStatus = waitpid(job->pid, nullptr,WNOHANG);
        if(waitStatus != 0) {
            jobs.erase(jobs.begin() + i);
        }
    }

    if(jobs.empty()) {
        job_counter = 0;
    }
    else {
        job_counter = jobs.back()->pid;
    }
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (JobEntry* job:jobs) {
        if (job->job_id == jobId) {
            return job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    for (auto it = jobs.begin(); it != jobs.end(); it++) {
        if ((*it)->job_id == jobId) {
            jobs.erase(it);
            return;
        }
    }
}

int JobsList::getMaxJobID() {
    int maxID = 0;
    for (JobEntry* job:jobs) {
        if (job->job_id > maxID) {
            maxID = job->job_id;
        }
    }

    return maxID;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobs.empty() || !lastJobId) {
        return nullptr;
    }
    *lastJobId = getMaxJobID();
    return getJobById(*lastJobId);
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    int index = 0, maxID = 0;
    JobEntry* lastStoppedJob = nullptr;
    for (JobEntry* job:jobs) {
        if ((job->job_id > maxID) && job->is_stopped) {
            maxID = job->job_id;
            lastStoppedJob = job;
        }
    }
    *jobId = maxID;
    return lastStoppedJob;
}

void JobsCommand::execute() {
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}

void ForegroundCommand::execute() {
    jobs->removeFinishedJobs();
    SmallShell& shell = SmallShell::getInstance();
    if (this->getArgByIndex(2) != nullptr) {
        std::cerr << "smash error: fg: invalid arguments" << std::endl;
    }

    JobsList::JobEntry* job = nullptr;
    if (this->getArgByIndex(1) != nullptr) {
        std::string jobID_argument = this->getArgByIndex(1);
        int job_id = std::stoi(jobID_argument); // convert argument string ti intger
        if (job_id == 0) {
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
        }

        job = jobs->getJobById(job_id);
        if (job == nullptr) {
            std::cerr << "smash error: fg: job-id " << jobID_argument << " does not exist" << std::endl;
            return;
        }
    }
    // if fg was typed with no arguments (without job-id) but the jobs list is empty
    else {
        if (this->jobs->getJobs().empty()) {
            std::cerr << "smash error: fg: jobs list is empty" << std::endl;
            return;
        }
        /*
        * if the job-id argument is not specified, then the job with
        * the maximal job id in the jobs list should be selected
        */
        int last_jobID = 0;
        job = jobs->getLastJob(&last_jobID);
    }

    if (job->is_stopped) {
        int killSYSCALL_result = kill(job->pid, SIGCONT);
        if (killSYSCALL_result == -1) {
            std::perror("smash error: kill  failed");
            return;
        }
    }

    std::cout << job->command->getCmdLine() << " : " << job->pid << std::endl;
    shell.setCurrentRunningJobPID(job->pid);
    shell.setCurrentCommandLine(std::string(job->command->getCmdLine()));

    int status = 0;
    int waitPID_result = waitpid(job->pid, &status, WUNTRACED);
    if (waitPID_result == -1) {
        std:perror("smash error: waitpid failed");
        return;
    }
}

void BackgroundCommand::execute() {
    jobs->removeFinishedJobs();
    SmallShell& shell = SmallShell::getInstance();
    if (this->getArgByIndex(2) != nullptr) {
        std::cerr << "smash error: bg: invalid arguments" << std::endl;
    }

    JobsList::JobEntry* job = nullptr;
    if (this->getArgByIndex(1) != nullptr) {
        std::string jobID_argument = this->getArgByIndex(1);
        int job_id = std::stoi(jobID_argument); // convert argument string ti intger
        if (job_id == 0) {
            std::cerr << "smash error: bg: invalid arguments" << std::endl;
        }

        job = jobs->getJobById(job_id);
        if (job == nullptr) {
            std::cerr << "smash error: bg: job-id " << jobID_argument << " does not exist" << std::endl;
            return;
        }
        if (!job->is_stopped) {
            std::cerr << "smash error: bg: job-id " << jobID_argument << " is already running in the background" << std::endl;
            return;
        }
    }

    // if bg was typed with no arguments (without job-id)
    else {
        /*
        * if the job-id argument is not specified, then the job with
        * the maximal job id in the jobs list should be selected
        */
        int last_jobID = 0;
        job = jobs->getLastJob(&last_jobID);
        if (job == nullptr) {
            std::cerr << "smash error: bg: there is no stopped jobs to resume" << std::endl;
            return;
        }
    }

    job->is_stopped = false;
    std::cout << job->command->getCmdLine() << " : " << job->pid << std::endl;

    int killSYSCALL_result = kill(job->pid, SIGCONT);
    if (killSYSCALL_result == -1) {
        std::perror("smash error: kill failed");
        return;
    }
}

void QuitCommand::execute() {
    if (this->getArgByIndex(1) && (strcmp(this->getArgByIndex(1), KILL) == 0)) {
        std::cout << "smash: sending SIGKILL signal to " << jobs->getJobs().size() << " jobs:" << endl;
        if (!jobs->getJobs().empty()) {
            std::sort(jobs->getJobs().begin(), jobs->getJobs().end(), PtrSort);
        }
        for (JobsList::JobEntry* job:jobs->getJobs()) {
            std::cout << job->pid << " : " << job->command->getCmdLine() << std::endl;
        }
        this->jobs->killAllJobs();
    }
    exit(0);
}

void KillCommand::execute() {
    this->jobs->removeFinishedJobs();
    int jobID = -1, signalNumber = -1;
    //char** smashArguments = new char * [COMMAND_MAX_ARGS];
    //int argumentsNumber= _parseCommandLine(this->getCmdLine(), smashArguments);

    if (this->getArgByIndex(2)) {
        jobID = atoi(this->getArgByIndex(2));
    }

    if (this->getArgByIndex(1)) {
        signalNumber = atoi(this->getArgByIndex(1)) * -1;
    }

    if (this->getArgByIndex(3) || jobID == -1 || signalNumber == -1 || signalNumber < 1 || signalNumber > 31) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    JobsList::JobEntry* job = jobs->getJobById(jobID);
    if (!job) {
        std::cerr << "smash error: kill: job-id " << jobID << " does not exist" << std::endl;
        return;
    }

    if(job->pid == -1) {
        std::cerr << "smash error: kill: job-id " << jobID << " does not exist" << std::endl;
        return;
    }

    if (kill(job->pid, signalNumber) == -1) {
        std::perror("smash error: kill failed");
        return;
    }

    std::cout << "signal number " << signalNumber <<  " was sent to pid " << job->pid << std::endl;
    this->jobs->removeFinishedJobs();
}

void ExternalCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid = fork();

    if (pid == -1) {
        std::perror("smash error: fork failed");
        return;
    }

    if (pid == 0) {
        int setGroup_result = setpgrp();
        if (setGroup_result == -1) {
            std::perror("smash error: setpgrp failed");
            return;
        }

        if (is_backGround) {
            _removeBackgroundSign(external_cmd_line);
        }

        int execlResult = execl("/bin/bash", "/bin/bash", "-c", external_cmd_line, nullptr);
        if (execlResult == -1) {
            perror("smash error: execl failed");
            return;
        }
    }
    else {
        int status = 0;

        if (!is_backGround) {
            shell.setCurrentRunningJobPID(pid);
            shell.setCurrentCommandLine(std::string(this->getCmdLine()));
            int waitResult = waitpid(pid, &status, WUNTRACED);
            if (waitResult == -1) {
                perror("smash error: waitpid failed");
                return;
            }
        }
        else {
            ExternalCommand* externalCmd = new ExternalCommand(this->getCmdLine(), jobs, is_backGround);
            jobs->addJob(externalCmd, pid, false);
        }
    }
}

void PipeCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    int fd_pipe[2];
    if (pipe(fd_pipe) == -1) {
        std::perror("smash error: pipe failed");
        return;
    }

    pid_t child1;
    pid_t child2;
    std::string command_line;

    if ((child1 = fork()) == 0) {
        setpgrp();
        if (close(fd_pipe[1]) == -1) {
            std::perror("smash error: close failed");
            return;
        }
        int stdin_fd = dup(0);
        if (stdin_fd == -1) {
            std::perror("smash error: dup failed");
            return;
        }

        if (dup2(fd_pipe[0],0) == -1){
            perror("smash error: dup2 failed");
            return;
        }

        if (std_type == STDERROR) {
            int begin = string(this->getCmdLine()).find_first_of(PIPE_TO_STDIN) + 2;
            command_line = _trim (string(this->getCmdLine()).substr(begin));
        }
        else {
            int begin = string((this->getCmdLine())).find_first_of(PIPE_TO_STDIN) + 1;
            command_line = _trim ((string(this->getCmdLine())).substr(begin));
        }

        char* line = new char[81];
        strcpy(line, command_line.c_str());
        Command* cmd = shell.CreateCommand(line);
        cmd->execute();
        delete cmd;

        if (dup2(stdin_fd,0) == -1) {
            std::perror("smash error: dup2 failed");
            return;
        }

        if (close(fd_pipe[0]) == -1) {
            std::perror("smash error: close failed");
            return;
        }
        exit(1);
    }

    //child2 = fork();
    else if ((child1 != -1) && ((child2 = fork()) == 0)) {
        setpgrp();
        if (close(fd_pipe[0]) == -1){
            std::perror("smash error: close failed");
            return;
        }

        int stdout_error_fd;
        if (std_type == STDERROR) {
            stdout_error_fd = dup(2);
            if(stdout_error_fd == -1) {
                std::perror("smash error: dup failed");
                return;
            }

            if (dup2(fd_pipe[1], 2) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
        }
        else {
            stdout_error_fd = dup(1);
            if(stdout_error_fd == -1) {
                std::perror("smash error: dup failed");
                return;
            }
            if (dup2(fd_pipe[1], 1) == -1){
                std::perror("smash error: dup2 failed");
                return;
            }
        }

        int end = (string(this->getCmdLine())).find_first_of("PIPE_TO_STDIN");
        command_line = _trim (string(this->getCmdLine()).substr(0, end));
        char* line = new char[81];
        strcpy(line, command_line.c_str());
        Command* cmd = shell.CreateCommand(line);
        cmd->execute();
        delete cmd;

        if (std_type == STDERROR){
            if (dup2(stdout_error_fd, 2) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
        }
        else {
            if (dup2(stdout_error_fd, 1) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
        }
        close(fd_pipe[1]);
        exit(1);
    }
    else {
        if (child1 == -1 || child2 == -1){
            std::perror("smash error: fork failed");
        }

        close(fd_pipe[0]);
        close(fd_pipe[1]);
        waitpid(child1, nullptr, 0);
        waitpid(child2, nullptr, 0);
    }
}

void RedirectionCommand::getDataFromRedirection(std::string cmd) {
    if (cmd.empty()) {
        return;
    }

    if(cmd.find(OUTPUT_APPEND) != -1) {
        is_output_append = true;
    }
    else {
        is_output_append = false;
    }

    int i = 0;
    while(cmd[i] != '>') {
        i++;
    }

    int begin;
    if(is_output_append) {
        begin = i + 2;
    }
    else {
        begin = i + 1;
    }

    string path_temp = cmd.substr(begin, cmd.size() - begin + 1);
    string command_temp = cmd.substr(0,i);

    current_destination = _trim(path_temp);
    current_command_line = _trim(command_temp);
}

void RedirectionCommand::prepare() {
    getDataFromRedirection(std::string(this->getCmdLine()));
    dup_stdout = dup(1);

    if(dup_stdout == (-1)) {
        std::perror("smash error: dup failed");
        return;
    }

    if(close(1) == (-1)) {
        close(dup_stdout);
        std::perror("smash error: close failed");
        return;
    }

    int current_fd = -1;
    if(!is_output_append) {
        current_fd = open(current_destination.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0655);
        if(current_fd == -1) {
            if(dup2(dup_stdout, 1) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
            if(close(dup_stdout) == -1) {
                std::perror("smash error: close failed");
                return;
            }
            dup_stdout = -1;
            is_failed = true;
            std::perror("smash error: open failed");
            return;
        }
    }

    else {
        current_fd = open(current_destination.c_str(), O_CREAT|O_RDWR|O_APPEND, 0655);
        if(current_fd == -1) {
            if(dup2(dup_stdout, 1) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
            if(close(dup_stdout) == -1) {
                std::perror("smash error: close failed");
                return;
            }

            dup_stdout = -1;
            is_failed = true;
            std::perror("smash error: open failed");
            return;
        }
    }

    if(dup(current_fd) == -1) {
        cleanup();
        is_failed = true;
        std::perror("smash error: dup failed");
        return;
    }

    is_failed = false;
}

void RedirectionCommand::cleanup() {
    if(close(1) == -1) {
        std::perror("smash error: close failed");
        return;
    }

    if(dup2(dup_stdout, 1) == -1) {
        std::perror("smash error: dup2 failed");
        return;
    }

    if(close(dup_stdout) == -1) {
        std::perror("smash error: close failed");
        return;
    }

    dup_stdout = -1;
}

void RedirectionCommand::execute() {
    prepare();

    if(is_failed) {
        return;
    }
    SmallShell& shell = SmallShell::getInstance();
    shell.executeCommand(this->current_command_line.c_str());
    cleanup();
}

void TimedJobs::removeKilledJobs() {
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid_smash = getpid();

    if(pid_smash != shell.getSmashPID()) {
        return;
    }

    for(unsigned int i = 0; i < shell.getTimedJobs()->timeouts.size(); i++){
        TimedEntry* timedEntry = shell.getTimedJobs()->timeouts[i];
        time_t curr_time = time(nullptr);

        if(timedEntry->getEndTime() <= curr_time) {
            delete shell.getTimedJobs()->timeouts[i];
            shell.getTimedJobs()->timeouts.erase(shell.getTimedJobs()->timeouts.begin() + i);
        }
    }
    shell.jobs.finishedJobs();
}

void TimedJobs::modifyJobByID(pid_t job_pid) {
    int timed_jobs_number = timeouts.size();
    timeouts[timed_jobs_number -1 ]->setPid(job_pid);
    std::sort(timeouts.begin(), timeouts.end(), timeoutEntryIsBigger);
}

void TimeoutCommand::getDataFromTimeOutCommand(const char *cmd) {
    if (!cmd) {
        return;
    }

    if (this->getArgumentsNumber() < 3) {
        error_type = TOO_FEW_ARGS;
        return;
    }

    std::string timeoutDuration = string(this->getArgByIndex(1));
    for (unsigned int index = 0; index < timeoutDuration.size(); index++) {
        if (timeoutDuration[index] < '0' || timeoutDuration[index] > '9') {
            error_type = TIMEOUT_NUMBER_IS_NOT_INTEGER;
            return;
        }
    }

    timer = atoi(this->getArgByIndex(1));
    string cmd_temp;

    for (int i = 2; i < this->getArgumentsNumber(); i++) {
        cmd_temp += this->getArgByIndex(i);
        cmd_temp += " ";
    }
    command = _trim(cmd_temp);
}

void TimeoutCommand::execute() {
    getDataFromTimeOutCommand(this->getCmdLine());

    if (error_type == TOO_FEW_ARGS) {
        std::cerr<< "smash error: timeout: too few arguments" << std::endl;
    }
    if(error_type == TIMEOUT_NUMBER_IS_NOT_INTEGER) {
        cerr << "smash error: timeout: bad arguments" << endl;
    }

    SmallShell& shell = SmallShell::getInstance();
    TimedEntry* current_timed_out = new TimedEntry(command, -1, time(nullptr), timer, false);

    shell.getTimedJobs()->getTimeOutsVector().push_back(current_timed_out);
    shell.setAlarmedJobs(true);

    char cmd_with_not_bg[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_with_not_bg,command.c_str());
    _removeBackgroundSign(cmd_with_not_bg);

    bool is_bg = _isBackgroundComamnd(this->getCmdLine());
    pid_t pid = fork();
    if(pid < 0) {
        std::perror("smash error: fork failed");
        return;
    }
    else if(pid == 0) {
        setpgrp();
        shell.executeCommand(cmd_with_not_bg);
        while(wait(nullptr) != -1) {}
        exit(0);
    }
    else {
        if(shell.getSmashPID() == getpid()) {
            shell.setAlarmedJobs(true);
            shell.getTimedJobs()->modifyJobByID(pid);
            time_t current_time = time(nullptr);
            TimedEntry* alarm_entry = shell.getTimedJobs()->getTimeOutsVector()[0];
            time_t end_time = alarm_entry->getEndTime();
            unsigned int seconds = end_time - current_time;
            alarm(seconds);

            if (is_bg) {
                shell.jobs.addJob(this,pid,false);
            }
            else {
                if(waitpid(pid, nullptr,WUNTRACED) == -1) {
                    std::perror("smash error: waitpid failed");
                    return;
                }
            }
            shell.setAlarmedJobs(false);
        }
    }
    shell.setAlarmedJobs(false);
}

SmallShell::SmallShell() : smash_pid(getpid()), current_prompt("smash"), last_dir_path(nullptr), current_running_jobPID(-1),
                cuurent_command_line(), timed_jobs(new TimedJobs()), is_alarmed_job(false) {}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    if (!cmd_line) {
        return nullptr;
    }

    char* args_making[COMMAND_MAX_ARGS + 1] = {nullptr};
    _parseCommandLine(cmd_line, args_making);

    std::string string_cmd_line = string(cmd_line);

    if((string_cmd_line.find(OUTPUT_OVERRIDE)) != string::npos) {
        return new RedirectionCommand(cmd_line);
    }
    else if(string_cmd_line.find(PIPE_TO_STDERROR) != string::npos) {
        return new PipeCommand(cmd_line, STDIN);
    }
    else if(string_cmd_line.find(PIPE_TO_STDIN) != string::npos){
        return new PipeCommand(cmd_line, STDERROR);
    }

    else if (strcmp(args_making[0], CHPROMPT) == 0) {
        return new ChangePromptCommand(cmd_line);
    }

    else if (strcmp(args_making[0], SHOWPID) == 0) {
        return new ShowPidCommand(cmd_line);
    }

    else if (strcmp(args_making[0], PWD) == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    else if (strcmp(args_making[0], CD) == 0) {
        return new ChangeDirCommand(cmd_line, &last_dir_path);
    }

    else if (strcmp(args_making[0], JOBS) == 0) {
        return new JobsCommand(cmd_line, &jobs);
    }

    else if (strcmp(args_making[0], FG) == 0) {
        return new ForegroundCommand(cmd_line, &jobs);
    }

    else if (strcmp(args_making[0], BG) == 0) {
        return new BackgroundCommand(cmd_line, &jobs);
    }

    else if (strcmp(args_making[0], QUIT) == 0) {
        return new QuitCommand(cmd_line, &jobs);
    }

    else if (strcmp(args_making[0], KILL) == 0) {
        return new KillCommand(cmd_line, &jobs);
    }
    else if (strcmp(args_making[0], TIMEOUT) == 0) {
        return new TimeoutCommand(cmd_line);
    }

    else {
        bool is_backGround = _isBackgroundComamnd(cmd_line);
        return new ExternalCommand(cmd_line, &jobs, is_backGround);
    }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    jobs.removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    if (cmd) {
        cmd->execute();
    }
    delete cmd;
    current_running_jobPID = -1;
    jobs.removeFinishedJobs();
}

