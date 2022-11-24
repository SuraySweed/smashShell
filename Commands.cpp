#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

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
    /*
    SmallShell& shell = SmallShell::getInstance();
    char buffer[FILENAME_MAX];
    getcwd(buffer, FILENAME_MAX);
    */

    char buffer[FILENAME_MAX];
    getCurrentDirectoryName(buffer);
    std::cout << buffer << std::endl;
}

void ChangeDirCommand::execute() {
    if (this->getArgByIndex(2) != nullptr) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
    }

    // there is '-' in the cd command
    char current_directory[FILENAME_MAX] = {0};

    if (strcmp(this->getArgByIndex(1), "-") == 0) {
        // the last working directory was empty
        if(this->last_directory_path.empty()) {
            std::cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        //set the current directory to the last one
        getCurrentDirectoryName(current_directory);

        if (chdir(this->last_directory_path.c_str()) == -1) {
            perror("smash error: chdir failed");
        }

        last_directory_path = std::string(current_directory);
        return;
    }
    else {
        getCurrentDirectoryName(current_directory);
        if (chdir(this->getArgByIndex(1)) == -1) {
            perror("smash error: chdir failed");
        }

        last_directory_path = std::string(current_directory);
        return;
    }
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    JobEntry* job = new JobEntry(job_counter + 1, pid, isStopped, cmd);
    job_counter += 1;
    jobs.push_back(job);
}

void JobsList::printJobsList() {
    for (JobEntry* job:jobs) {
        std::cout << "[" << job->job_id << "] " << job->command->getCmdLine() << " : " << job->pid << " " <<
        (int) difftime(time(nullptr), job->elapsed_time) << " secs " << (job->is_stopped ? "(stopped)" : "") << std::endl;
    }
}

void JobsList::killAllJobs() {
    removeFinishedJobs();
    for (JobEntry* job:jobs) {
        if (kill(job->pid, SIGKILL) < 0) {
            perror("smash error: kill failed");
        }
    }
    if (!jobs.empty()) {
        jobs.clear();
        //removeFinishedJobs();
    }
}

void JobsList::removeFinishedJobs() {
    int index = 0;
    for (JobEntry* job:jobs) {
        int res = waitpid(job->pid, nullptr, WNOHANG);
        if (res != 0) {
            jobs.erase(jobs.begin() + index);
        }
        index++;
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

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobs.empty() || !lastJobId) {
        return nullptr;
    }
    //removeFInshiedJobs..
    *lastJobId = job_counter;
    return getJobById(job_counter);
}

// ask for this function, compare with time or with ID
JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    return nullptr;
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
    if (this->getArgByIndex(2)) {
        jobID = atoi(this->getArgByIndex(2));
    }

    if (this->getArgByIndex(1)) {
        signalNumber = atoi(this->getArgByIndex(2));
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

SmallShell::SmallShell() : smash_pid(getpid()), current_prompt("smash"), last_dir_path(), current_running_jobPID(-1), cuurent_command_line() {}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    char* args_making[COMMAND_MAX_ARGS + 1] = {nullptr};
    _parseCommandLine(cmd_line, args_making);

    if (strcmp(args_making[0], CHPROMPT) == 0) {
        return new ChangePromptCommand(cmd_line);
    }

    else if (strcmp(args_making[0], SHOWPID) == 0) {
        return new ShowPidCommand(cmd_line);
    }

    else if (strcmp(args_making[0], PWD) == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    else if (strcmp(args_making[0], CD) == 0) {
        return new ChangePromptCommand(cmd_line);
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




/*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
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