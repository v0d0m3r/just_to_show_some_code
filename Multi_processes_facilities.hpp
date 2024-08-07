#ifndef MULTI_PROCESSES_FACILITIES_HPP
#define MULTI_PROCESSES_FACILITIES_HPP

//------------------------------------------------------------------------------

#include "Cashbox_lib_facilities.hpp"
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

//------------------------------------------------------------------------------

namespace Cashbox {

//------------------------------------------------------------------------------

std::string get_waitpid_errno_descritpion(std::string&& title,
                                          const int error_code);

//------------------------------------------------------------------------------

std::string get_fork_errno_descritpion(std::string&& title,
                                          const int error_code);

//------------------------------------------------------------------------------

template <class Duration_type = std::chrono::seconds,
          class Clock = std::chrono::high_resolution_clock>
class Simple_timer
{
    using time_point_t = typename Clock::time_point;
    time_point_t start_{Clock::now()};
    time_point_t end_{};
public:
    void tick()
    {
        end_ = time_point_t{};
        start_ = Clock::now();
    }

    void tock() { end_ = Clock::now(); }

    template <class T = Duration_type>
    auto duration() const
    {
        if (end_ == time_point_t{})
            error("toc before reporting");
        return std::chrono::duration_cast<T>(end_ - start_);
    }
};

//------------------------------------------------------------------------------

enum class Exit_code_waiting_for_child {
    success, expired
};

//------------------------------------------------------------------------------
// Ожидает завершения дочернего процесса
// если превышено timeout, то пытается удалить дочерний процесс
// Возвращает: success, если процесс завершился успешно; иначе expired
// Генерирует исключение, если waitpid завершился с ошибкой
// Гарантия безопасности: общая
template<class Duration_type>
auto waiting_for_child_process(const pid_t child_pid, const Duration_type timeout)
{
    auto exit_code{Exit_code_waiting_for_child::success};
    int child_status;
    Simple_timer<Duration_type> timer;
    constexpr int waitpid_error_code{-1};
    int res{0};
    int options{WNOHANG};
    do  {
        if (timer.tock(); timer.duration() >= timeout) {
            exit_code = Exit_code_waiting_for_child::expired;
            kill(child_pid, SIGKILL);
            options = 0;                // Делаем waitpid блокирующим
                                        // чтобы дождаться результата
            std::this_thread::sleep_for(100ms);
        }
        errno = 0;
        res = waitpid(child_pid, &child_status, options);
        if      (res == waitpid_error_code)
            error(get_waitpid_errno_descritpion("waiting_for_child_process", errno));
        else if (res == child_pid)
            break;
        else if (res != 0)  // should never be in there
            error("waiting_for_child_process(): unspecified state!");

        std::this_thread::sleep_for(100ms);
    } while (true);
    return exit_code;
}

//------------------------------------------------------------------------------

template<class T>
bool am_i_child(const T pid)
{
    return pid == 0;
}

//------------------------------------------------------------------------------

template<class T>
void make_non_blocking(const T& raii)
{
    const auto fd{fileno(raii.get())};

    auto flags{fcntl(fd, F_GETFL, 0)};
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

//------------------------------------------------------------------------------

template<class F, class... A>
auto spawn_task(F&& f, A&&... a)
{
    auto res{f.get_future()};
    std::thread t(std::move(f), std::move<A>(a)...);
    t.detach();
    return res;
}

//------------------------------------------------------------------------------

enum class Execute_command_exit_code {
    success, expired
};

//------------------------------------------------------------------------------
// Выполняет команду (может использоваться в потоке)
template<class Duration_type = std::chrono::seconds>
struct Execute_command {
    using result_type = std::pair<Execute_command_exit_code, std::string>;

    Execute_command(std::string&& command, const Duration_type timeout)
        : command_{std::move(command)}, timeout_{timeout}
    {}

    void operator() ();
    std::future<result_type> get_future()
        { return honest_promise_.get_future(); }
private:
    std::promise<result_type> honest_promise_;
    const std::string command_;
    const Duration_type timeout_;
};

//------------------------------------------------------------------------------

template<class... Ts>
auto make_pipe(Ts&&... params)
{
    std::unique_ptr<FILE, decltype(&pclose)>
            pipe(popen(std::forward<Ts>(params)...), pclose);
    return pipe;
}

//------------------------------------------------------------------------------
// Может заблокироваться на pclose когда pipe покидает область видимости
// Помещает в promise пару код завершения и id_process
// id_process может быть: пустая строка - нет процесов или выход по таймауту
//                        один или несколько чисел > 0 - есть процессы
template<class Duration_type>
void Execute_command<Duration_type>::operator()()
try {
    auto pipe{make_pipe(command_.c_str(), "r")};
    if (!pipe)
        throw std::runtime_error(std::string{"popen() failed! errno: "}
                                 + strerror(errno));
    make_non_blocking(pipe);

    Simple_timer<Duration_type> timer;
    std::array<char, 128> buffer;
    std::string result;
    while (true) {
        if (timer.tock(); timer.duration() >= timeout_) {
            honest_promise_.set_value(
                        {Execute_command_exit_code::expired, std::move(result)});
            return;
        }
        errno = 0;
        const auto answer{fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr};
        if (!answer) {
            if (errno != EWOULDBLOCK)
                break;      // Произошла ошибка или пайп завершил работу
            continue;       // EWOULDBLOCK, значит поток в нормальном состоянии
        }
        result += buffer.data();
        this_thread::sleep_for(100ms);
    }
    honest_promise_.set_value({Execute_command_exit_code::success,
                               std::move(result)});
}
catch (...) {
    honest_promise_.set_exception(std::current_exception());
}

//------------------------------------------------------------------------------
// Простое убивание процесса и ожидание, без проверки кодов ошибок
template<class T>
void kill_and_wait(const T process_id)
{
    kill(process_id, SIGKILL);
    int child_status;
    waitpid(process_id, &child_status, 0);  // Блокирующий вызов
}

//------------------------------------------------------------------------------

// Убивает или ждет внешний процесс/комманду
void handle_other_external_processes(Cashbox::Logger_wrap& lout,
                                     const std::string& process_name);

//------------------------------------------------------------------------------

inline const char* get_c_str(const std::string& s)
{
    if (s.empty())
        return nullptr;
    return s.c_str();
}

//------------------------------------------------------------------------------

struct Execl_wrapper {
    Execl_wrapper(const fs::path& path, std::string&& operation_type)
        : path_to_module{path}, oper_type{std::move(operation_type)} {}

    explicit Execl_wrapper(const fs::path& path)
        : path_to_module{path}, oper_type{} {}

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        if (!oper_type.empty())
            execl(path_to_module.c_str(), path_to_module.c_str(), oper_type.c_str(),
                  std::forward<Ts>(ts)..., nullptr);
        else // аргументы для комманды пока перадаются как std::string
             // соотвественно приводим к const char*
            execl(path_to_module.c_str(), path_to_module.c_str(),
                  get_c_str(std::forward<Ts>(ts))..., nullptr);
    }

private:
    const fs::path& path_to_module;
    std::string oper_type;
};

//------------------------------------------------------------------------------

}

//------------------------------------------------------------------------------

#endif // MULTI_PROCESSES_FACILITIES_HPP
