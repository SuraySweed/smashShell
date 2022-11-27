#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

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
  // TODO: Add your implementation
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

