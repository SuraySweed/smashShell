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

static bool isStringNumber(const string& str)
{
    for (char const &ch : str) {
        if (std::isdigit(ch) == 0) {
            return false;
        }
    }
    return true;
}

static bool PtrSort(JobsList::JobEntry* job1, JobsList::JobEntry* job2) { return (*job1 < *job2); }

Command::Command(const char *cmd_line) {
    this->cmd_line = new char[strlen(cmd_line) + 1];
    strcpy(this->cmd_line, cmd_line);
    arguments_number = _parseCommandLine(cmd_line, args);
}

Command::Command(const Command &other) {
    cmd_line = new char[strlen(other.cmd_line) + 1];
    strcpy(cmd_line, other.cmd_line);
    arguments_number = _parseCommandLine(other.cmd_line, args);
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

        if (chdir(*(this->last_directory_path)) == -1) {
            perror("smash error: chdir failed");
            return;
        }

        *last_directory_path = current_directory;
        return;
    }
    else {
        getCurrentDirectoryName(current_directory);
        if (chdir(this->getArgByIndex(1)) == -1) {
            perror("smash error: chdir failed");
            return;
        }
        *last_directory_path = current_directory;
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
    std::sort(jobs.begin(), jobs.end(), PtrSort);
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

JobsList::JobEntry *JobsList::getJobByPID(int jobPID) {
    for (JobEntry* job:jobs) {
        if (job->pid == jobPID) {
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
        return;
    }

    JobsList::JobEntry* job = nullptr;
    if (this->getArgByIndex(1) != nullptr) {
        std::string jobID_argument = this->getArgByIndex(1);

        if (jobID_argument[0] == '-') {
            std::string str_temp = jobID_argument.substr(1, jobID_argument.size() - 1);
            if (!isStringNumber(str_temp)) {
                std::cerr << "smash error: fg: invalid arguments" << std::endl;
                return;
            }
        }
        else if (!isStringNumber(jobID_argument)) {
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
        }
        int job_id = std::stoi(jobID_argument); // convert argument string to integer
        if (job_id == 0) {
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
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
            std::perror("smash error: kill failed");
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
        return;
    }

    JobsList::JobEntry* job = nullptr;
    if (this->getArgByIndex(1) != nullptr) {
        std::string jobID_argument = this->getArgByIndex(1);

        int job_id = std::stoi(jobID_argument); // convert argument string to integer
        if (job_id == 0) {
            std::cerr << "smash error: bg: invalid arguments" << std::endl;
            return;
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
        job = jobs->getLastStoppedJob(&last_jobID);
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
            //std::sort(jobs->getJobs().begin(), jobs->getJobs().end(), PtrSort);
        }
        for (JobsList::JobEntry* job:jobs->getJobs()) {
            std::cout << job->pid << ": " << job->command->getCmdLine() << std::endl;
        }
        this->jobs->killAllJobs();
    }
    exit(0);
}

void KillCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    this->jobs->removeFinishedJobs();
    if (this->getArgumentsNumber() < 3 || this->getArgumentsNumber() > 3) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    int jobID = -111, signalNumber = -111; // -111  EROOR

    try {
        jobID = stoi(this->getArgByIndex(2));//convert string to int to get the job_id
    } catch(exception &e) {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    if (this->getArgByIndex(1)[0] != '-') {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    try {
        signalNumber = stoi(string(this->getArgByIndex(1)).substr(1, string(this->getArgByIndex(1)).size() - 1));
    } catch(exception &e) {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    if (jobID == -111 || jobID == 0 || signalNumber == -111 || signalNumber < 0 || signalNumber > 31) {
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
    else {
        std::cout << "signal number " << signalNumber << " was sent to pid " << job->pid << std::endl;
        this->jobs->removeFinishedJobs();
        if(signalNumber == SIGKILL) {
            if(waitpid(job->pid, NULL, 0) == -1) {
                std::perror("smash error: waitpid failed");
                return;
            }

            shell.jobs.removeJobById(jobID);
        }
    }
}

ExternalCommand::ExternalCommand(const char *cmd_line, JobsList *jobs, bool is_bg) : Command(cmd_line), jobs(jobs),
    is_backGround(is_bg), is_complex_command(isComplexCommand()) {}


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
        if (is_complex_command) {
            char* destination = new char[81];
            strcpy(destination, this->getCmdLine());

            char* argv[] = {(char*)"/bin/bash", (char*)"-c", destination, nullptr};
            int execvResult = execv("/bin/bash", argv);
            if (execvResult == -1) {
                perror("smash error: execv failed");
                return;
            }
        }
        else {
            char* cmd = new char[81];
            strcpy(cmd, this->getCmdLine());

            if (is_backGround) {
                _removeBackgroundSign(cmd);
            }
            char* args[COMMAND_MAX_ARGS + 1] = {nullptr};
            _parseCommandLine(cmd, args);

            int execvpResult = execvp(args[0], args);
            if (execvpResult == -1) {
                delete[] cmd;
                perror("smash error: execvp failed");
                return;
            }
            delete[] cmd;
        }
    }
    else {
        if (!is_backGround) {
            int status = 0;
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

void PipeCommand::prepare() {
    size_t pipe_index = string(this->getCmdLine()).find(PIPE_TO_STDIN);
    if (std_type == STDERROR) {
        pipe_index = string(this->getCmdLine()).find(PIPE_TO_STDERROR);
    }
    string cmd_splitted1 = string(this->getCmdLine());
    string cmd_splitted2 = string(this->getCmdLine());

    command1 = cmd_splitted1.substr(0, pipe_index);
    pipe_index++;
    if (std_type == STDERROR) {
        pipe_index++;
    }

    string str_temp = cmd_splitted2.substr(pipe_index, cmd_splitted1.length() - pipe_index);
    str_temp= _trim(str_temp);
    str_temp += " ";
    command2 = str_temp.substr(0, str_temp.find_last_of(WHITESPACE));
}

void PipeCommand::execute() {
    this->prepare();
    SmallShell& shell = SmallShell::getInstance();
    int fd_pipe[2];

    if(pipe(fd_pipe) == -1) {
        std::perror("smash error: pipe failed");
        return;
    }

    pid_t child1_pid = fork();
    if(child1_pid == -1){
        std::perror("smash error: fork failed");
        return;
    }
    if (child1_pid == 0) {
        int setGroup_result = setpgrp();
        if (setGroup_result == -1) {
            std::perror("smash error: setpgrp failed");
            return;
        }

        if(std_type == STDERROR) {
            if(dup2(fd_pipe[1], 2) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
            if(close(fd_pipe[0])==-1) {
                std::perror("smash error: close failed");
                return;
            }
            if(close(fd_pipe[1])==-1) {
                std::perror("smash error: close failed");
                return;
            }
        }

        else {
            if(dup2(fd_pipe[1], 1) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }
            if(close(fd_pipe[0]) == -1) {
                std::perror("smash error: close failed");
                return;
            }
            if(close(fd_pipe[1]) == -1) {
                std::perror("smash error: close failed");
                return;
            }
        }

        Command* pipe_command = shell.CreateCommand(command1.c_str());
        pipe_command->execute();
        exit(0);
    }

    pid_t child2_pid = fork();
    if(child2_pid == -1){
        std::perror("smash error: fork failed");
        return;
    }

    if (child2_pid == 0) {
        if( setpgrp()==-1){
            std::perror("smash error: setpgrp failed");
            return;
        }
        if(dup2(fd_pipe[0], 0) == -1) {
            std::perror("smash error: dup2 failed");
            return;
        }
        if( close(fd_pipe[0]) == -1) {
            std::perror("smash error: close failed");
            return;
        }
        if(close(fd_pipe[1]) == -1) {
            std::perror("smash error: close failed");
            return;
        }

        Command* my_command = shell.CreateCommand(command2.c_str());
        my_command->execute();
        exit(0);
    }

    if(close(fd_pipe[0]) == -1) {
        std::perror("smash error: close failed");
        return;
    }
    if(close(fd_pipe[1])) {
        std::perror("smash error: close failed");
        return;
    }


    if(waitpid(child1_pid, NULL, WUNTRACED) == -1) {
        std::perror("smash error: waitpid failed");
        return;
    }
    if(waitpid(child2_pid, NULL, WUNTRACED)==-1){
        std::perror("smash error: waitpid failed");
        return;
    }
    return;
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

    for(unsigned int i = 0; i < shell.timed_jobs->timeouts.size(); i++){
        TimedEntry* timedEntry = shell.timed_jobs->timeouts[i];
        time_t curr_time = time(nullptr);

        if(timedEntry->getEndTime() <= curr_time) {
            delete shell.timed_jobs->timeouts[i];
            shell.timed_jobs->timeouts.erase(shell.timed_jobs->timeouts.begin() + i);
        }
    }
    shell.jobs.finishedJobs();
}

void TimedJobs::modifyJobByID(pid_t job_pid) {
    int timed_jobs_number = timeouts.size();
    if (timed_jobs_number >= 1) {
        timeouts[timed_jobs_number -1]->setPid(job_pid);
    }
    else {
        timeouts[timed_jobs_number]->setPid(job_pid); //timeouts[0] = ....
    }
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

    /*
    if (error_type == TOO_FEW_ARGS) {
        std::cerr << "smash error: timeout: too few arguments" << std::endl;
        return;
    }
    if(error_type == TIMEOUT_NUMBER_IS_NOT_INTEGER) {
        cerr << "smash error: timeout: bad arguments" << endl;
        return;
    }
    */
    if (error_type == TOO_FEW_ARGS || error_type == TIMEOUT_NUMBER_IS_NOT_INTEGER) {
        std::cerr << "smash error: timeout: invalid arguments" << std::endl;
        return;
    }

    SmallShell& shell = SmallShell::getInstance();
    TimedEntry* current_timed_out = new TimedEntry(command, -1, time(nullptr), timer, false);

    shell.timed_jobs->timeouts.push_back(current_timed_out);
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
            shell.timed_jobs->modifyJobByID(pid);
            time_t current_time = time(nullptr);
            TimedEntry* alarm_entry = shell.timed_jobs->timeouts[0];
            time_t end_time = alarm_entry->getEndTime();
            unsigned int seconds = end_time - current_time;
            alarm(seconds);

            if (is_bg) {
                TimeoutCommand* copy_command = new TimeoutCommand(*this);
                shell.jobs.addJob(copy_command, pid, false);
                //shell.jobs.addJob(this, pid, false);
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

void SetcoreCommand::execute() {
    if (this->getArgumentsNumber() != 3) {
        std::cerr << "smash error: setcore: invalid arguments" << "\r\n";
        return;
    }

    SmallShell& smash = SmallShell::getInstance();
    try {
        JobsList::JobEntry* job = smash.jobs.getJobById(stoi(this->getArgByIndex(1)));
        if(!job) {
            std::cerr << "smash error: setcore: job-id "<< this->getArgByIndex(1) << " does not exist" << "\r\n";
            return;
        }
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(stoi(this->getArgByIndex(2)), &mask);
        int schedResult = sched_setaffinity(job->pid, sizeof(mask), &mask);
        if(schedResult == -1){
            cerr<<"smash error: setcore: invalid core number"<<"\r\n";
            return;
        }
    } catch(exception &e) {
        std::cerr << "smash error: setcore: invalid arguments" << "\r\n";
        return;
    }
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
    if (string_cmd_line.empty()) { return nullptr; }

    if((string_cmd_line.find(OUTPUT_OVERRIDE)) != string::npos) {
        return new RedirectionCommand(cmd_line);
    }
    else if(string_cmd_line.find(PIPE_TO_STDERROR) != string::npos) {
        return new PipeCommand(cmd_line, STDERROR);
    }
    else if(string_cmd_line.find(PIPE_TO_STDIN) != string::npos){
        return new PipeCommand(cmd_line, STDIN);
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
    else if (strcmp(args_making[0], SET_CORE) == 0) {
        return new SetcoreCommand(cmd_line);
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

