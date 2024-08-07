// this is pseudo code :)
#include "Multi_processes_facilities.hpp"

enum class ExecuteUnderlyingAppResult {
    been_executed, chosen_exit, none_inputed, out_of_range, not_supported
};

ExecuteUnderlyingAppResult ExecuteUnderlyingApp()
{
    lout(Lg_lvl::debug4) << "ExecuteUnderlyingApp(): start";
    const auto [result, args] {input_args_for_some_proc(lout)};
    if (result != ExecuteUnderlyingAppResult::been_executed)
        return result;
    handle_other_external_processes(lout, "some_proc");

    constexpr pid_t fork_error_result{-1};
    errno = 0;
    const auto pid{fork()};
    if (pid == fork_error_result)
        error(get_fork_errno_descritpion(std::string{"fork(): "},
                                         errno));
    if (am_i_child(pid)) {
        Execl_wrapper execl_wrap{path_to_some_random_bank_, std::string{args.front()}};
        auto argument{std::string{}};
        for (auto begin{args.begin()+1}; begin != args.end(); ++begin) {
            argument += *begin;
            argument += " ";
        }
        if (!argument.empty())
            argument.pop_back();
        execl_wrap(argument.c_str());
    }
    // I am parent
    const auto exit_code{waiting_for_child_process(pid, 300s)};
    if (exit_code == Exit_code_waiting_for_child::expired)
        lout(Lg_lvl::warning) << "ExecuteUnderlyingApp(): waiting "
                                 "for some proc - timeout expired!";
    copy_slip_for_execute_underlying_app(Kind_cashless::some_random_bank);
    lout(Lg_lvl::debug4) << "ExecuteUnderlyingApp(): finish";
    return ExecuteUnderlyingAppResult::been_executed;
}
