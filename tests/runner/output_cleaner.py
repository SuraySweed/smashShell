#! /usr/bin/python3

import sys
import re

# possible lines:
# std::cout << job->m_pid << ": " << job->m_command->getCmdLine() << std::endl;
# std::cout << "smash pid is " << SmallShell::getInstance().getPid() << std::endl;
# std::cout << "signal number " << m_signum << " was sent to pid " << job.getPid() << std::endl;
# std::cout << "[" << job->m_id << "] " << job->m_command->getCmdLine() << " : " << job->getPid() << " "
#   << time << "secs" << stopped << std::endl;
# std::cout << "process " << job->getPid() << " was stopped" << std::endl;
# std::cout << job.getCmdLine() << " : " << pid << std::endl;
# std::cout << "signal number " << m_signum << " was sent to pid " << job.getPid() << std::endl;


SHOWPID_REGEX = r".*smash pid is (\d+)\n"
QUIT_KILL_REGEX = r".*smash: sending SIGKILL signal to \d jobs:\n"
PID_EXTRACTOR_REGEX = r"(signal number \d+ was sent to pid (\d+)\n)|"\
    "(\[\d+\] .* : (\d+) \d+ secs.*\n)|"\
    "(process (\d+) was stopped\n)|"\
    "(process (\d+) was killed\n)|"\
    "(signal number \d was sent to pid (\d+)\n)|"\
    "(smash> .* : (\d+)\n)"  # fg/bg
TIMEZONE_REGEX = r"(\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d+ \+)(\d+)"


def get_pids_match(fname, pids, counter=2):
    with open(fname, "r") as file:
        end = False
        for line in file:
            m = re.match(SHOWPID_REGEX, line)
            if m:
                pids.setdefault(m.groups()[0], "1")
            if end:
                pid, _ = line.split(":", maxsplit=1)
                if pid not in pids:
                    pids[pid] = str(counter)
                    counter += 1
            if re.match(QUIT_KILL_REGEX, line):
                end = True
            else:
                for result in re.findall(PID_EXTRACTOR_REGEX, line):
                    for pid in result[1::2]:
                        if pid and pid not in pids:
                            pids[pid] = str(counter)
                            counter += 1
    return counter


def clean_file(fname, pids):
    with open(fname, "r") as f_in:
        with open(fname + ".processed", "w") as f_out:
            for line in f_in:
                line = re.sub(
                    r"(\[\d+\] .* : \d+) \d+ (secs.*)", r"\1 X \2", line)
                for real, fake in pids.items():
                    line = line.replace(real, fake)
                line = re.sub(TIMEZONE_REGEX, r"\1XXXX", line)
                f_out.write(line)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: ./{sys.argv[0]} <output_file> <error_file>")
        return
    _, stdout_file, stderr_file = sys.argv
    pids = {}
    get_pids_match(stderr_file, pids, get_pids_match(stdout_file, pids))
    clean_file(stdout_file, pids)
    clean_file(stderr_file, pids)


if __name__ == "__main__":
    main()
