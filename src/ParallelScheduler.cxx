#include "ParallelScheduler.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <chrono>
#include <map>
#include <iostream>

namespace parallel {

SchedulerResult ParallelScheduler::run(const ParallelBlock& block, WorkerFunction fn) {
    SchedulerResult result;
    if (block.tasks.empty()) return result;

    auto wall_start = std::chrono::steady_clock::now();
    ResourceBudget budget(block.options);

    struct RunningWorker {
        size_t task_index;
        std::chrono::steady_clock::time_point start_time;
    };

    std::map<pid_t, RunningWorker> running;
    size_t next_task = 0;
    bool abort_flag = false;

    sigset_t block_mask, prev_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGTERM);
    sigaddset(&block_mask, SIGINT);

    auto try_dispatch = [&]() {
        while (next_task < block.tasks.size() && !abort_flag) {
            const auto& task = block.tasks[next_task];
            if (!budget.can_dispatch(task)) break;

            budget.acquire(task);
            auto task_start = std::chrono::steady_clock::now();

            sigprocmask(SIG_BLOCK, &block_mask, &prev_mask);
            pid_t pid = fork();
            if (pid == 0) {
                struct sigaction sa = {};
                sa.sa_handler = SIG_DFL;
                sigaction(SIGTERM, &sa, nullptr);
                sigaction(SIGINT, &sa, nullptr);

                int devnull = open("/dev/null", O_WRONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }

                sigprocmask(SIG_SETMASK, &prev_mask, nullptr);

                int rc = fn(task);
                _exit(rc);
            }
            sigprocmask(SIG_SETMASK, &prev_mask, nullptr);

            if (pid > 0) {
                running[pid] = {next_task, task_start};
                next_task++;
            } else {
                budget.release(task);
                PluginResult pr;
                pr.name = task.name;
                pr.exit_code = -1;
                result.failed.push_back(pr);
                next_task++;
                if (block.options.fail_mode == FailMode::Fast) {
                    abort_flag = true;
                }
            }
        }
    };

    auto kill_all_running = [&]() {
        for (auto& [pid, w] : running) {
            kill(pid, SIGTERM);
        }
        for (auto& [pid, w] : running) {
            int st;
            waitpid(pid, &st, 0);
        }
        running.clear();
    };

    try_dispatch();

    while (!running.empty()) {
        int status;
        pid_t finished = waitpid(-1, &status, 0);
        if (finished <= 0) continue;

        auto it = running.find(finished);
        if (it == running.end()) continue;

        size_t idx = it->second.task_index;
        double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - it->second.start_time).count();

        const auto& task = block.tasks[idx];
        budget.release(task);
        running.erase(it);

        PluginResult pr;
        pr.name = task.name;
        pr.elapsed_seconds = elapsed;
        pr.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        if (pr.exit_code == 0) {
            result.completed.push_back(pr);
        } else {
            result.failed.push_back(pr);
            if (block.options.fail_mode == FailMode::Fast) {
                abort_flag = true;
                kill_all_running();
                break;
            }
        }

        try_dispatch();
    }

    result.total_elapsed_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - wall_start).count();
    return result;
}

} // namespace parallel
