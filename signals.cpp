#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <string.h>
#include <unistd.h>

using namespace std;

void ctrlZHandler(int sig_num) {
    SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash: got ctrl-Z" << std::endl;

    if(shell.getCurrentRunningJobPID() != -1) {
        if(killpg(shell.getCurrentRunningJobPID(), SIGTSTP) == -1) {
            std::perror("smash error: killpg failed");
            return;
        }

        JobsList::JobEntry* currentJob = shell.jobs.getJobById(shell.getCurrentRunningJobPID());
        if(currentJob) {
            currentJob->is_stopped = true;
            currentJob->elapsed_time = time(nullptr);
        }
        else {
            Command* cmd = shell.CreateCommand(shell.getCurrentCmdLine().c_str());
            shell.jobs.addJob(cmd, shell.getCurrentRunningJobPID(), true);
        }
        std::cout << "smash: process " << shell.getCurrentRunningJobPID() << " was stopped" << std::endl;
        shell.setCurrentRunningJobPID(-1);
    }
}

void ctrlCHandler(int sig_num) {
    SmallShell& shell = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    if (shell.getCurrentRunningJobPID() != -1) {
        if (killpg(shell.getCurrentRunningJobPID(), SIGKILL) == -1) {
            perror("smash error: killpg failed");
            return;
        }
        JobsList::JobEntry* job = shell.jobs.getJobById(shell.getCurrentRunningJobPID());
        if (job) {
            shell.jobs.removeJobById(job->job_id);
        }
        cout << "smash: process " << shell.getCurrentRunningJobPID() << " was killed" << endl;
        shell.setCurrentRunningJobPID(-1);
    }
}

void alarmHandler(int sig_num) {
    SmallShell& shell = SmallShell::getInstance();
    cout << "smash: got an alarm" << endl;
    TimedEntry* job_to_kill = shell.getTimedJobs()->getTimeOutsVector()[0];
    if(job_to_kill == nullptr) {
        cout << "is null" << endl;
    }
    pid_t pid_to_kill = job_to_kill->getPid();
    if(pid_to_kill == shell.getSmashPID()) { return; }
    int killSTATUS = killpg(pid_to_kill,SIGKILL);
    if(killSTATUS == -1) {
        perror("smash error: killpg failed");
    }
    cout << "smash: timeout " << job_to_kill->getTimer() << " " << job_to_kill->getCommandLine() << " timed out!" << endl;
    shell.getTimedJobs()->removeKilledJobs();
    shell.jobs.finishedJobs();
    if(shell.getTimedJobs()->getTimeOutsVector().empty()) {
        return;
    }
    TimedEntry* curr_alarmed_job = shell.getTimedJobs()->getTimeOutsVector()[0];
    time_t curr_alarm = curr_alarmed_job->getEndTime();
    alarm(curr_alarm-time(nullptr));
}

