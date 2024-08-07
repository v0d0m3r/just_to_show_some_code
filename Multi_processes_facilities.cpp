#include "Multi_processes_facilities.hpp"

//------------------------------------------------------------------------------

namespace Cashbox {

//------------------------------------------------------------------------------

std::string get_waitpid_errno_descritpion(std::string&& title,
                                          const int error_code)
{
    std::string description{std::move(title)};
    switch (error_code) {
    case EAGAIN:
        description += "The PID file descriptor specified in id "
                       "is nonblocking and the process that it refers "
                       "to has not terminated.";
        break;
    case ECHILD:
        description += "The process specified by pid does not exist "
                       "or is not a child of the calling process."
                       "(This can  happen for one's own child if the "
                       "action for SIGCHLD is set to SIG_IGN.  "
                       "See also the Linux Notes section about threads.)";
        break;
    case EINTR:
        description += "WNOHANG was not set and an unblocked signal or a "
                       "SIGCHLD was caught;";
        break;
    case EINVAL:
        description += "The options argument was invalid.";
        break;
    case ESRCH:
        description += "pid is equal to INT_MIN";
        break;
    default:
        description += "unnown error " + std::string{strerror(errno)};
        break;
    }
    return description;
}

//------------------------------------------------------------------------------

std::string get_fork_errno_descritpion(std::string&& title,
                                          const int error_code)
{
    std::string description{std::move(title)};
    switch (error_code) {
    case ENOMEM:
        description +=  "failed to allocate the necessary kernel "
                        "structures because memory is tight";
        break;
    case EAGAIN:
        description += "A system-imposed limit on the number of "
                       "threads was encountered.";
        break;
    case ENOSYS:
        description += "is not supported on this platform (e.g, "
                       "hardware without a Memory-Management Unit).";
        break;
    default:
        description += "unnown error " + std::string{strerror(errno)};
        break;
    }
    return description;
}

//------------------------------------------------------------------------------

void handle_other_external_processes(Cashbox::Logger_wrap& lout,
                                     const std::string& process_name)
try {
    std::ostringstream command;
    command << R"(result=$(ps axo pid=,stat=,command= | grep )"
            << process_name
            << R"( | awk '$3 !~ /^(grep|sh)/ { print $1 }') ; echo ${result})";
    Execute_command exec_com{command.str(), 3s};
    auto result{spawn_task(std::move(exec_com))};
    auto [exit_code, process_ids] = result.get();
    if (exit_code == Execute_command_exit_code::expired)
        lout(Lg_lvl::warning) << "handle_other_external_processes():  search of "
                              << process_name << " processes is failed due to timeout!";
    lout(Lg_lvl::debug4) << "handle_other_external_processes(): exit code: "
                         << to_utype(exit_code) << "; result: " << process_ids;
    std::stringstream ss{std::move(process_ids)};
    for (pid_t process_id{0}; ss >> process_id;) {
        lout(Lg_lvl::info) << "handle_other_external_processes(): process_id "
                              "to kill_and_wait: " << process_id;
        kill_and_wait(process_id);
    }
}
catch (const std::exception& e) {
    lout(Lg_lvl::error) << "handle_other_external_processes(): " << e.what();
}
catch (...) {
    lout(Lg_lvl::error) << "handle_other_external_processes(): unknown exception";
}

//------------------------------------------------------------------------------

}
