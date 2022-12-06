#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

#define CHPROMPT "chprompt"
#define SHOWPID "showpid"
#define PWD "pwd"
#define CD "cd"
#define JOBS "jobs"
#define KILL "kill"
#define FG "fg"
#define BG "bg"
#define QUIT "quit"
#define TIMEOUT "timeout"
#define PIPE_TO_STDERROR "|&"
#define PIPE_TO_STDIN "|"
#define OUTPUT_APPEND ">>"
#define OUTPUT_OVERRIDE ">"

class Command {
private:
    char* cmd_line;
    char* args[COMMAND_MAX_ARGS + 1] = {nullptr}; //[0]- command
    int arguments_number;

public:
    Command(const char* cmd_line);
    Command(const Command& other);
    Command& operator=(const Command& command);
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    char* getCmdLine() { return cmd_line; }
    char* getArgByIndex(int i) { return args[i]; }
    char** getCommandArguments() { return args; }
    int getArgumentsNumber() { return arguments_number; }
};

class JobsList;

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
    virtual ~BuiltInCommand() = default;

    virtual void execute() = 0; //pure function
};

class ExternalCommand : public Command {
    JobsList* jobs;
    bool is_backGround;
    bool is_complex_command;
    //std::string external_cmd_line;

    bool isComplexCommand() {
        std::string command_str = std::string(this->getCmdLine());
        return ((command_str.find("*") != std::string::npos) || (command_str.find("?") != std::string::npos));
    }

public:
    ExternalCommand(const char* cmd_line, JobsList* jobs, bool is_bg);
    virtual ~ExternalCommand() = default;
    void execute() override;
};

enum pipeType {STDERROR, STDIN};

class PipeCommand : public Command {
private:
    pipeType std_type;
public:
    PipeCommand(const char* cmd_line, pipeType type) : Command(cmd_line), std_type(type) {}
    virtual ~PipeCommand() = default;
    void execute() override;
};

class RedirectionCommand : public Command {
private:
    std::string current_destination;
    std::string current_command_line;
    int dup_stdout;
    bool is_failed;
    bool is_output_append;

    void getDataFromRedirection(std::string cmd);

public:
    explicit RedirectionCommand(const char* cmd_line) : Command(cmd_line), current_destination(), current_command_line(), dup_stdout(-1) {}
    virtual ~RedirectionCommand() = default;
    void prepare();
    void cleanup();
    void execute() override;

};

class ChangePromptCommand : public BuiltInCommand {
public:
    ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ChangePromptCommand() = default;
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
    char** last_directory_path;
    char* current_directory;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), last_directory_path(plastPwd) {
        current_directory = new char[FILENAME_MAX];
    }
    virtual ~ChangeDirCommand() = default;
    void execute() override;

};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~GetCurrDirCommand() = default;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
  virtual ~ShowPidCommand() = default;
  void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t pid;
        time_t elapsed_time;
        bool is_stopped;
        Command* command;

        JobEntry(int jobID, pid_t pid, bool isStopped, Command* cmd) : job_id(jobID), pid(pid),
                elapsed_time(time(nullptr)), is_stopped(isStopped), command(cmd) {}
        JobEntry& operator=(const JobEntry& job_entry);
        JobEntry(const JobEntry& other) = default;
        ~JobEntry() = default;
        bool operator<(const JobsList::JobEntry& job) const { return (this->job_id < job.job_id); }
        bool operator>(const JobsList::JobEntry& job) const { return (this->job_id > job.job_id); }
        bool operator==(const JobsList::JobEntry& job) const { return (this->job_id == job.job_id); }
  };
private:
    std::vector<JobEntry*> jobs;
    int job_counter = 0; // equal to job max id
public:
    JobsList() = default;
    JobsList(const JobsList& other) = default;
    JobsList& operator=(const JobsList& job_list) = default;
    ~JobsList() = default;
    void addJob(Command* cmd, pid_t pid, bool isStopped);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    void finishedJobs();
    JobEntry * getJobById(int jobId);
    JobEntry * getJobByPID(int jobPID);
    void removeJobById(int jobId);
    int getMaxJobID();
    JobEntry * getLastJob(int* lastJobId);
    JobEntry *getLastStoppedJob(int *jobId);
    std::vector<JobEntry*> getJobs() { return jobs; }
};

class JobsCommand : public BuiltInCommand {
private:
    JobsList* jobs;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
  virtual ~JobsCommand() = default;
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
  virtual ~ForegroundCommand() = default;
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~BackgroundCommand() = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobs;
    //static bool PtrSort(JobsList::JobEntry* job1, JobsList::JobEntry* job2) { return (*job1 < *job2); }

public:
    QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~QuitCommand() = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~KillCommand() = default;
    void execute() override;
};

class TimedEntry {
private:
    std::string command;
    pid_t pid;
    time_t start_time;
    time_t end_time;
    int timer;
    bool is_foreground;
    bool is_killed;

public:
    TimedEntry(std::string cmd, pid_t pid, time_t start_time, int timer, bool is_fg) : command(cmd), pid(pid), start_time(start_time),
            end_time(start_time + timer), timer(timer), is_foreground(is_fg), is_killed(false) {}
    ~TimedEntry() = default;
    time_t getEndTime() { return end_time; }
    pid_t getPid() { return pid; }
    pid_t setPid(pid_t id) { pid = id; }
    int getTimer() { return timer; }
    std::string getCommandLine() { return command; }
};

class TimedJobs {
public:
    std::vector<TimedEntry*> timeouts;

    TimedJobs() = default;
    ~TimedJobs() = default;

    static bool timeoutEntryIsBigger(TimedEntry* t1, TimedEntry* t2) {
        if((t1->getEndTime()) >= (t2->getEndTime())) {
            return false;
        }
        return true;
    }
    void removeKilledJobs();
    void modifyJobByID(pid_t job_pid);
    //std::vector<TimedEntry*> getTimeOutsVector() { return timeouts; }
};

enum TimeOutErrorType {SUCCESS, TOO_FEW_ARGS, TIMEOUT_NUMBER_IS_NOT_INTEGER};

class TimeoutCommand : public BuiltInCommand {
    int timer;
    TimeOutErrorType error_type;
    std::string command;

    void getDataFromTimeOutCommand(const char* cmd);

public:
    explicit TimeoutCommand(const char* cmd_line) : BuiltInCommand(cmd_line), timer(0), error_type(SUCCESS), command() {}
    TimeoutCommand(const TimeoutCommand& other) = default;
    virtual ~TimeoutCommand() = default;
    void execute() override;
};

class FareCommand : public BuiltInCommand {
  /* Optional */
  // TODO: Add your data members
 public:
  FareCommand(const char* cmd_line);
  virtual ~FareCommand() {}
  void execute() override;
};

class SetcoreCommand : public BuiltInCommand {
  /* Optional */
  // TODO: Add your data members
 public:
  SetcoreCommand(const char* cmd_line);
  virtual ~SetcoreCommand() {}
  void execute() override;
};


class SmallShell {
 private:
    pid_t smash_pid;                    // for show pid
    std::string current_prompt;         // for chprompt
    char* last_dir_path;                // for cd
    pid_t current_running_jobPID;
    std::string cuurent_command_line;
    bool is_alarmed_job;

    SmallShell();

 public:

    JobsList jobs;
    TimedJobs* timed_jobs;

    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell() = default;
    void executeCommand(const char* cmd_line);

    std::string getCurrentPrompt() { return current_prompt; }
    void setCurrentPrompt(std::string prompt_str) { current_prompt = prompt_str; }

    pid_t getSmashPID() { return smash_pid; }
    void setCurrentRunningJobPID(pid_t jobPID) { current_running_jobPID = jobPID; }
    void setCurrentCommandLine(std::string cmd_line) { cuurent_command_line = cmd_line; }

    pid_t getCurrentRunningJobPID() { return current_running_jobPID; }
    std::string getCurrentCmdLine() { return cuurent_command_line; }

    //TimedJobs* getTimedJobs() { return timed_jobs; }
    bool isAlarmedJobs() { return is_alarmed_job; }
    void setAlarmedJobs(bool is_alarmed) {is_alarmed_job = is_alarmed; }
};

#endif //SMASH_COMMAND_H_
