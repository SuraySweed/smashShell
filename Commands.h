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
#define PIPE_TO_STDERROR "|&"
#define PIPE_TO_STDIN "|"
#define OUTPUT_APPEND ">>"
#define OUTPUT_OVERRIDE ">"

class Command {
private:
    char* cmd_line;
    char* args[COMMAND_MAX_ARGS + 1] = {nullptr}; //[0]- command

public:
    Command(const char* cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    char* getCmdLine() { return cmd_line; }
    char* getArgByIndex(int i) { return args[i]; }
    char** getCommandArguments() { return args; }
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
    char* external_cmd_line;
public:
    ExternalCommand(const char* cmd_line, JobsList* jobs, bool is_bg) : Command(cmd_line), jobs(jobs), is_backGround(is_bg) {
        //external_cmd_line = new char[81];
        //strcpy(external_cmd_line, this->getCmdLine());
        external_cmd_line = this->getCmdLine();
    }
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
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
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

        JobEntry(int jobID, pid_t pid, bool isStopped, Command* cmd) : job_id(jobID),
            pid(pid), elapsed_time(time(nullptr)), is_stopped(isStopped), command(cmd) {}
        ~JobEntry() = default;
        ////// we have to implemenet operator=
  };
private:
    std::vector<JobEntry*> jobs;
    int job_counter = 1;
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(Command* cmd, pid_t pid, bool isStopped);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
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

class TimeoutCommand : public BuiltInCommand {
/* Optional */
// TODO: Add your data members
 public:
  explicit TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
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

    SmallShell();

 public:

    JobsList jobs;

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
};

#endif //SMASH_COMMAND_H_
