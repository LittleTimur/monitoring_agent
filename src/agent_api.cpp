#include "agent_api.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cpr/cpr.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <random>
#include <functional>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows_metrics_collector.hpp"
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#endif

namespace agent {



static std::string current_iso_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    
    // Добавляем миллисекунды
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::string result(buf);
    result += "." + std::to_string(ms.count()).substr(0, 3);
    
    return result;
}

static void append_audit(const agent::AgentConfig& cfg, const std::string& line) {
    if (!cfg.audit_log_enabled) return;
    try {
        std::filesystem::path path = cfg.audit_log_path.empty() ? agent::AgentConfig::get_config_path("agent_audit.log") : cfg.audit_log_path;
        std::ofstream f(path, std::ios::app);
        if (!f.is_open()) return;
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        f << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << " " << line << "\n";
    } catch (...) {}
}

// Forward declaration required before first use
static bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& path);

// Function to clean invalid UTF-8 bytes from string
static std::string clean_utf8(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (c < 0x80) {
            // ASCII character - всегда безопасно
            result.push_back(static_cast<char>(c));
            i++;
        } else if (c < 0xC2) {
            // Invalid UTF-8 start byte - заменяем на пробел
            result.push_back(' ');
            i++;
        } else if (c < 0xE0) {
            // 2-byte UTF-8 sequence
            if (i + 1 < str.length() && (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80) {
                // Проверяем, что это валидная последовательность
                if (c >= 0xC2) { // Минимальное значение для 2-байтовой последовательности
                    result.push_back(str[i]);
                    result.push_back(str[i + 1]);
                    i += 2;
                } else {
                    result.push_back(' ');
                    i += 2;
                }
            } else {
                // Неполная последовательность - заменяем на пробел
                result.push_back(' ');
                i++;
            }
        } else if (c < 0xF0) {
            // 3-byte UTF-8 sequence
            if (i + 2 < str.length() && 
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) == 0x80) {
                // Проверяем валидность
                if (c >= 0xE0 && c <= 0xEF) {
                    result.push_back(str[i]);
                    result.push_back(str[i + 1]);
                    result.push_back(str[i + 2]);
                    i += 3;
                } else {
                    result.push_back(' ');
                    i += 3;
                }
            } else {
                // Неполная последовательность
                result.push_back(' ');
                i++;
            }
        } else if (c < 0xF5) {
            // 4-byte UTF-8 sequence
            if (i + 3 < str.length() && 
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i + 3]) & 0xC0) == 0x80) {
                // Проверяем валидность
                if (c >= 0xF0 && c <= 0xF4) {
                    result.push_back(str[i]);
                    result.push_back(str[i + 1]);
                    result.push_back(str[i + 2]);
                    result.push_back(str[i + 3]);
                    i += 4;
                } else {
                    result.push_back(' ');
                    i += 4;
                }
            } else {
                // Неполная последовательность
                result.push_back(' ');
                i++;
            }
        } else {
            // Invalid UTF-8 start byte - заменяем на пробел
            result.push_back(' ');
            i++;
        }
    }
    
    return result;
}

// Function to check UTF-8 validity
static bool is_valid_utf8(const std::string& str) {
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (c < 0x80) {
            // ASCII character
            i++;
        } else if (c < 0xC2) {
            // Invalid UTF-8 start byte
            return false;
        } else if (c < 0xE0) {
            // 2-byte UTF-8 sequence
            if (i + 1 >= str.length()) return false;
            if ((static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) return false;
            i += 2;
        } else if (c < 0xF0) {
            // 3-byte UTF-8 sequence
            if (i + 2 >= str.length()) return false;
            if ((static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) return false;
            i += 3;
        } else if (c < 0xF5) {
            // 4-byte UTF-8 sequence
            if (i + 3 >= str.length()) return false;
            if ((static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            // Invalid UTF-8 start byte
            return false;
        }
    }
    return true;
}

CommandResponse AgentManager::handle_push_script(const Command& cmd) {
    try {
        std::string name = cmd.data.value("name", "");
        std::string content = cmd.data.value("content", "");
        if (name.empty() || content.empty()) return CommandResponse{false, "name and content required", {}, current_iso_time()};
        
        // Дополнительная проверка длины имени файла
        if (name.length() > 255) {
            return CommandResponse{false, "Script name too long (max 255 characters)", {}, current_iso_time()};
        }
        
        // Дополнительная проверка размера содержимого
        if (content.length() > 1024 * 1024) { // 1MB limit
            return CommandResponse{false, "Script content too large (max 1MB)", {}, current_iso_time()};
        }
        
        // Очищаем содержимое скрипта от проблемных UTF-8 символов
        std::string cleaned_content = clean_utf8(content);
        
        std::filesystem::path base = AgentConfig::get_scripts_path(config_.scripts_dir);
        
        // Безопасное создание директории
        try {
            std::filesystem::create_directories(base);
        } catch (const std::exception& e) {
            return CommandResponse{false, "Cannot create scripts directory: " + std::string(e.what()), {}, current_iso_time()};
        }
        
        std::filesystem::path target = base / name;
        if (!is_subpath(base, target)) return CommandResponse{false, "Invalid target path", {}, current_iso_time()};
        
        // Безопасное создание файла
        std::ofstream f(target, std::ios::binary);
        if (!f.is_open()) return CommandResponse{false, "Cannot open file for write", {}, current_iso_time()};
        
        try {
            f.write(cleaned_content.data(), static_cast<std::streamsize>(cleaned_content.size()));
            f.close();
        } catch (const std::exception& e) {
            f.close();
            std::filesystem::remove(target); // Удаляем частично созданный файл
            return CommandResponse{false, "Error writing script content: " + std::string(e.what()), {}, current_iso_time()};
        }
        
#ifndef _WIN32
        if (cmd.data.contains("chmod")) {
            try {
                std::string mode = cmd.data["chmod"].get<std::string>();
                std::string cmdline = "chmod " + mode + " '" + target.string() + "'";
                std::system(cmdline.c_str());
            } catch (const std::exception& e) {
                // Игнорируем ошибки chmod, но логируем
                std::cerr << "Warning: chmod failed: " << e.what() << std::endl;
            }
        }
#endif
        
        nlohmann::json data; 
        data["path"] = target.string();
        
        // Безопасное логирование
        try {
            append_audit(config_, std::string("PUSH_SCRIPT ") + target.string());
        } catch (...) {
            // Игнорируем ошибки логирования
            std::cerr << "Warning: Failed to write to audit log" << std::endl;
        }
        
        return CommandResponse{true, "Script saved", data, current_iso_time()};
    } catch (const std::exception& e) {
        std::cerr << "Error in handle_push_script: " << e.what() << std::endl;
        return CommandResponse{false, std::string("Error push_script: ") + e.what(), {}, current_iso_time()};
    } catch (...) {
        std::cerr << "Unknown error in handle_push_script" << std::endl;
        return CommandResponse{false, "Unknown error in push_script", {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_list_scripts(const Command& cmd) {
    try {
        std::filesystem::path base = AgentConfig::get_scripts_path(config_.scripts_dir);
        nlohmann::json arr = nlohmann::json::array();
        if (std::filesystem::exists(base)) {
            for (auto& p : std::filesystem::directory_iterator(base)) {
                if (!p.is_regular_file()) continue;
                nlohmann::json j;
                j["name"] = p.path().filename().string();
                j["size"] = static_cast<int64_t>(std::filesystem::file_size(p));
                arr.push_back(j);
            }
        }
        nlohmann::json data; data["scripts"] = arr;
        return CommandResponse{true, "Scripts listed", data, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error list_scripts: ") + e.what(), {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_delete_script(const Command& cmd) {
    try {
        std::string name = cmd.data.value("name", "");
        if (name.empty()) return CommandResponse{false, "name is required", {}, current_iso_time()};
        std::filesystem::path base = AgentConfig::get_scripts_path(config_.scripts_dir);
        std::filesystem::path target = base / name;
        if (!is_subpath(base, target)) return CommandResponse{false, "Invalid target path", {}, current_iso_time()};
        if (std::filesystem::exists(target)) {
            std::filesystem::remove(target);
            append_audit(config_, std::string("DELETE_SCRIPT ") + target.string());
            return CommandResponse{true, "Deleted", {}, current_iso_time()};
        }
        return CommandResponse{false, "Not found", {}, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error delete_script: ") + e.what(), {}, current_iso_time()};
    }
}

static bool is_allowed_interpreter(const std::vector<std::string>& allowlist, const std::string& name) {
    for (const auto& it : allowlist) {
        if (it == name) return true;
    }
    return false;
}

static bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& path) {
    try {
        auto abs_base = std::filesystem::weakly_canonical(base);
        auto abs_path = std::filesystem::weakly_canonical(path);
        auto itb = abs_base.begin();
        auto itp = abs_path.begin();
        for (; itb != abs_base.end() && itp != abs_path.end(); ++itb, ++itp) {
            if (*itb != *itp) return false;
        }
        return std::distance(abs_base.begin(), abs_base.end()) <= std::distance(abs_path.begin(), abs_path.end());
    } catch (...) {
        return false;
    }
}

static std::string substitute_params(const std::string& templ,
                                     const std::vector<std::string>& params) {
    std::string out = templ;
    for (int i = 1; i <= 9; ++i) {
        std::string needle = "$" + std::to_string(i);
        const std::string replacement = (i - 1 < static_cast<int>(params.size())) ? params[i - 1] : std::string("");
        size_t pos = 0;
        while ((pos = out.find(needle, pos)) != std::string::npos) {
            out.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        }
    }
    return out;
}

#ifdef _WIN32
ProcessResult run_process_windows(const std::vector<std::string>& argv,
                                  const std::unordered_map<std::string, std::string>& env,
                                  const std::string& working_dir,
                                  int timeout_sec,
                                  int max_output_bytes,
                                  const std::function<bool()>& is_cancelled = {}) {
    ProcessResult result;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE out_read = NULL, out_write = NULL;
    HANDLE err_read = NULL, err_write = NULL;
    if (!CreatePipe(&out_read, &out_write, &sa, 0)) {
        result.exit_code = -1;
        result.combined_output = "CreatePipe failed";
        return result;
    }
    if (!CreatePipe(&err_read, &err_write, &sa, 0)) {
        result.exit_code = -1;
        result.combined_output = "CreatePipe (stderr) failed";
        CloseHandle(out_read); CloseHandle(out_write);
        return result;
    }
    if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0)) {
        result.exit_code = -1;
        result.combined_output = "SetHandleInformation failed";
        CloseHandle(out_read); CloseHandle(out_write); CloseHandle(err_read); CloseHandle(err_write);
        return result;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = err_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};

    // Build command line
    std::ostringstream oss;
    for (size_t i = 0; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        bool need_quotes = a.find(' ') != std::string::npos || a.find('\"') != std::string::npos;
        if (i) oss << ' ';
        if (need_quotes) {
            oss << '"' << a << '"';
        } else {
            oss << a;
        }
    }
    std::string cmd_line = oss.str();
    std::vector<char> cmd_line_buf(cmd_line.begin(), cmd_line.end());
    cmd_line_buf.push_back('\0');

    // Build environment block by merging current env with overrides
    std::vector<char> env_block_buf;
    LPCH cur_env = GetEnvironmentStringsA();
    std::vector<std::string> env_strings;
    if (cur_env) {
        LPCSTR p = cur_env;
        while (*p) {
            std::string s = p;
            env_strings.push_back(s);
            p += s.size() + 1;
        }
    }
    // Build case-insensitive map for overrides
    auto to_upper = [](std::string s){ for (auto& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c))); return s; };
    std::unordered_map<std::string, size_t> name_to_index;
    for (size_t i = 0; i < env_strings.size(); ++i) {
        const std::string& s = env_strings[i];
        auto pos = s.find('=');
        if (pos == std::string::npos) continue;
        std::string name = s.substr(0, pos);
        name_to_index[to_upper(name)] = i;
    }
    for (const auto& kv : env) {
        std::string name_upper = to_upper(kv.first);
        std::string pair = kv.first + "=" + kv.second;
        auto it = name_to_index.find(name_upper);
        if (it != name_to_index.end()) {
            env_strings[it->second] = pair;
        } else {
            env_strings.push_back(pair);
        }
    }
    if (cur_env) FreeEnvironmentStringsA(cur_env);
    if (!env_strings.empty()) {
        size_t total = 1; // final \0
        for (const auto& s : env_strings) total += s.size() + 1;
        env_block_buf.resize(total, '\0');
        size_t off = 0;
        for (const auto& s : env_strings) {
            std::memcpy(env_block_buf.data() + off, s.c_str(), s.size());
            off += s.size();
            env_block_buf[off++] = '\0';
        }
        env_block_buf[off] = '\0';
    }

    BOOL ok = CreateProcessA(
        NULL,
        cmd_line_buf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        env_block_buf.empty() ? NULL : env_block_buf.data(),
        (working_dir.empty() ? NULL : working_dir.c_str()),
        &si,
        &pi
    );

    CloseHandle(out_write); // child inherits; parent reads
    CloseHandle(err_write);

    if (!ok) {
        result.exit_code = -1;
        result.combined_output = "CreateProcess failed";
        CloseHandle(out_read);
        return result;
    }

    DWORD wait_ms = timeout_sec > 0 ? static_cast<DWORD>(timeout_sec * 1000) : INFINITE;
    DWORD start_tick = GetTickCount();
    std::string out_buf; out_buf.reserve(4096);
    std::string err_buf; err_buf.reserve(4096);
    std::string comb; comb.reserve(8192);
    char tmp[4096];
    DWORD bytes_read = 0;

    // Read loop while waiting
    for (;;) {
        // Non-blocking peek
        DWORD available = 0;
        if (PeekNamedPipe(out_read, NULL, 0, NULL, &available, NULL) && available) {
            if (ReadFile(out_read, tmp, sizeof(tmp), &bytes_read, NULL) && bytes_read > 0) {
                size_t to_copy = std::min<size_t>(bytes_read, std::max(0, max_output_bytes - static_cast<int>(out_buf.size())));
                out_buf.append(tmp, tmp + to_copy);
                comb.append(tmp, tmp + to_copy);
                if (out_buf.size() >= static_cast<size_t>(max_output_bytes)) result.truncated = true;
            }
        }
        available = 0;
        if (PeekNamedPipe(err_read, NULL, 0, NULL, &available, NULL) && available) {
            if (ReadFile(err_read, tmp, sizeof(tmp), &bytes_read, NULL) && bytes_read > 0) {
                size_t to_copy = std::min<size_t>(bytes_read, std::max(0, max_output_bytes - static_cast<int>(err_buf.size())));
                err_buf.append(tmp, tmp + to_copy);
                comb.append(tmp, tmp + to_copy);
                if (err_buf.size() >= static_cast<size_t>(max_output_bytes)) result.truncated = true;
            }
        }
        DWORD elapsed = GetTickCount() - start_tick;
        DWORD remain = (wait_ms == INFINITE) ? 100 : (elapsed >= wait_ms ? 0 : wait_ms - elapsed);
        if (is_cancelled && is_cancelled()) {
            TerminateProcess(pi.hProcess, 1);
            result.timed_out = false;
            break;
        }
        DWORD w = WaitForSingleObject(pi.hProcess, remain == 0 ? 0 : 50);
        if (w == WAIT_OBJECT_0) break;
        if (wait_ms != INFINITE && elapsed >= wait_ms) {
            TerminateProcess(pi.hProcess, 1);
            result.timed_out = true;
            break;
        }
    }

    // Drain remaining output
    while (ReadFile(out_read, tmp, sizeof(tmp), &bytes_read, NULL) && bytes_read > 0) {
        size_t to_copy = std::min<size_t>(bytes_read, std::max(0, max_output_bytes - static_cast<int>(out_buf.size())));
        out_buf.append(tmp, tmp + to_copy);
        comb.append(tmp, tmp + to_copy);
        if (out_buf.size() >= static_cast<size_t>(max_output_bytes)) { result.truncated = true; break; }
    }
    while (ReadFile(err_read, tmp, sizeof(tmp), &bytes_read, NULL) && bytes_read > 0) {
        size_t to_copy = std::min<size_t>(bytes_read, std::max(0, max_output_bytes - static_cast<int>(err_buf.size())));
        err_buf.append(tmp, tmp + to_copy);
        comb.append(tmp, tmp + to_copy);
        if (err_buf.size() >= static_cast<size_t>(max_output_bytes)) { result.truncated = true; break; }
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);
    
    // Clean invalid UTF-8 bytes from output
    result.stdout_output = clean_utf8(out_buf);
    result.stderr_output = clean_utf8(err_buf);
    result.combined_output = clean_utf8(comb);

    CloseHandle(out_read);
    CloseHandle(err_read);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return result;
}
#else
ProcessResult run_process_posix(const std::vector<std::string>& argv,
                                const std::unordered_map<std::string, std::string>& env,
                                const std::string& working_dir,
                                int timeout_sec,
                                int max_output_bytes,
                                const std::function<bool()>& is_cancelled = {}) {
    ProcessResult result;
    int outfd[2];
    int errfd[2];
    if (pipe(outfd) != 0 || pipe(errfd) != 0) {
        result.exit_code = -1;
        result.combined_output = "pipe failed";
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(outfd[0]); close(outfd[1]);
        close(errfd[0]); close(errfd[1]);
        result.exit_code = -1;
        result.combined_output = "fork failed";
        return result;
    }
    if (pid == 0) {
        // Child
        // Create new process group for easier kill
        setpgid(0, 0);
        dup2(outfd[1], STDOUT_FILENO);
        dup2(errfd[1], STDERR_FILENO);
        close(outfd[0]); close(outfd[1]);
        close(errfd[0]); close(errfd[1]);
        if (!working_dir.empty()) {
            chdir(working_dir.c_str());
        }
        for (const auto& kv : env) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    // Parent
    close(outfd[1]);
    close(errfd[1]);
    fcntl(outfd[0], F_SETFL, fcntl(outfd[0], F_GETFL) | O_NONBLOCK);
    fcntl(errfd[0], F_SETFL, fcntl(errfd[0], F_GETFL) | O_NONBLOCK);
    std::string out_buf; out_buf.reserve(4096);
    std::string err_buf; err_buf.reserve(4096);
    std::string comb; comb.reserve(8192);
    auto start = std::chrono::steady_clock::now();
    char tmp[4096];
    for (;;) {
        ssize_t n = read(outfd[0], tmp, sizeof(tmp));
        if (n > 0) {
            size_t to_copy = std::min<size_t>(n, std::max(0, max_output_bytes - static_cast<int>(out_buf.size())));
            out_buf.append(tmp, tmp + to_copy);
            comb.append(tmp, tmp + to_copy);
            if (out_buf.size() >= static_cast<size_t>(max_output_bytes)) result.truncated = true;
        }
        n = read(errfd[0], tmp, sizeof(tmp));
        if (n > 0) {
            size_t to_copy = std::min<size_t>(n, std::max(0, max_output_bytes - static_cast<int>(err_buf.size())));
            err_buf.append(tmp, tmp + to_copy);
            comb.append(tmp, tmp + to_copy);
            if (err_buf.size() >= static_cast<size_t>(max_output_bytes)) result.truncated = true;
        }

        int status = 0;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
            break;
        }
        if (is_cancelled && is_cancelled()) {
            kill(-pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
        if (timeout_sec > 0 && elapsed >= timeout_sec) {
            result.timed_out = true;
            kill(-pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Drain remaining
    for (;;) {
        ssize_t n = read(outfd[0], tmp, sizeof(tmp));
        if (n > 0) {
            size_t to_copy = std::min<size_t>(n, std::max(0, max_output_bytes - static_cast<int>(out_buf.size())));
            out_buf.append(tmp, tmp + to_copy);
            comb.append(tmp, tmp + to_copy);
            if (out_buf.size() >= static_cast<size_t>(max_output_bytes)) { result.truncated = true; break; }
        } else break;
    }
    for (;;) {
        ssize_t n = read(errfd[0], tmp, sizeof(tmp));
        if (n > 0) {
            size_t to_copy = std::min<size_t>(n, std::max(0, max_output_bytes - static_cast<int>(err_buf.size())));
            err_buf.append(tmp, tmp + to_copy);
            comb.append(tmp, tmp + to_copy);
            if (err_buf.size() >= static_cast<size_t>(max_output_bytes)) { result.truncated = true; break; }
        } else break;
    }
    close(outfd[0]);
    close(errfd[0]);
    
    // Clean invalid UTF-8 bytes from output (same as Windows version)
    result.stdout_output = clean_utf8(out_buf);
    result.stderr_output = clean_utf8(err_buf);
    result.combined_output = clean_utf8(comb);
    return result;
}
#endif


std::string AgentManager::generate_job_id() {
    static const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);
    std::string s(12, ' ');
    for (auto& c : s) c = chars[dis(gen)];
    return s;
}

CommandResponse AgentManager::handle_get_job_output(const Command& cmd) {
    try {
        std::string job_id;
        if (cmd.data.contains("job_id")) job_id = cmd.data["job_id"].get<std::string>();
        if (job_id.empty()) return CommandResponse{false, "job_id is required", {}, current_iso_time()};
        std::shared_ptr<BackgroundJobInfo> job;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto it = jobs_.find(job_id);
            if (it == jobs_.end()) return CommandResponse{false, "job not found", {}, current_iso_time()};
            job = it->second;
        }
        nlohmann::json data;
        data["job_id"] = job->job_id;
        data["completed"] = job->completed.load();
        data["timed_out"] = job->timed_out.load();
        data["exit_code"] = job->exit_code.load();
        data["duration_ms"] = static_cast<int64_t>(job->duration_ms);
        data["truncated"] = job->truncated.load();
        data["output"] = job->output;
        bool success = job->completed.load() ? (job->exit_code == 0) : true;
        return CommandResponse{success, job->completed.load() ? "Job completed" : "Job running", data, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error get_job_output: ") + e.what(), {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_kill_job(const Command& cmd) {
    try {
        std::string job_id;
        if (cmd.data.contains("job_id")) job_id = cmd.data["job_id"].get<std::string>();
        if (job_id.empty()) return CommandResponse{false, "job_id is required", {}, current_iso_time()};
        std::shared_ptr<BackgroundJobInfo> job;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto it = jobs_.find(job_id);
            if (it == jobs_.end()) return CommandResponse{false, "job not found", {}, current_iso_time()};
            job = it->second;
            job->cancel_requested = true;
        }
        append_audit(config_, std::string("JOB_KILL id=") + job_id);
        nlohmann::json data;
        data["job_id"] = job_id;
        data["cancel_requested"] = true;
        return CommandResponse{true, "Cancel requested", data, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error kill_job: ") + e.what(), {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_list_jobs(const Command& cmd) {
    try {
        purge_old_jobs();
        nlohmann::json arr = nlohmann::json::array();
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        for (const auto& [id, job] : jobs_) {
            nlohmann::json j;
            j["job_id"] = id;
            j["completed"] = job->completed.load();
            j["timed_out"] = job->timed_out.load();
            j["cancel_requested"] = job->cancel_requested.load();
            j["exit_code"] = job->exit_code.load();
            j["duration_ms"] = static_cast<int64_t>(job->duration_ms);
            j["truncated"] = job->truncated.load();
            j["started_at_sec"] = static_cast<int64_t>(job->started_at_sec);
            j["completed_at_sec"] = static_cast<int64_t>(job->completed_at_sec);
            arr.push_back(j);
        }
        nlohmann::json data;
        data["jobs"] = arr;
        return CommandResponse{true, "Jobs listed", data, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error list_jobs: ") + e.what(), {}, current_iso_time()};
    }
}

// Command implementation
Command Command::from_json(const nlohmann::json& j) {
    Command cmd;
    if (j.contains("command")) cmd.command = j["command"];
    if (j.contains("data")) cmd.data = j["data"];
    if (j.contains("timestamp")) cmd.timestamp = j["timestamp"];
    return cmd;
}

nlohmann::json Command::to_json() const {
    nlohmann::json j;
    j["command"] = command;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j;
}

// CommandResponse implementation
nlohmann::json CommandResponse::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["message"] = message;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j;
}

// AgentHttpServer implementation
AgentHttpServer::AgentHttpServer(const AgentConfig& config, AgentManager* manager) : config_(config), manager_(manager) {
    // Регистрируем настоящие обработчики команд
    register_command_handler("collect_metrics", [this](const Command& cmd) {
        return manager_->handle_collect_metrics(cmd);
    });
    register_command_handler("update_config", [this](const Command& cmd) {
        return manager_->handle_update_config(cmd);
    });
    register_command_handler("restart", [this](const Command& cmd) {
        return manager_->handle_restart(cmd);
    });
    register_command_handler("stop", [this](const Command& cmd) {
        return manager_->handle_stop(cmd);
    });
    register_command_handler("run_script", [this](const Command& cmd) {
        return manager_->handle_run_script(cmd);
    });
    register_command_handler("get_job_output", [this](const Command& cmd) {
        return manager_->handle_get_job_output(cmd);
    });
    register_command_handler("kill_job", [this](const Command& cmd) {
        return manager_->handle_kill_job(cmd);
    });
    register_command_handler("list_jobs", [this](const Command& cmd) {
        return manager_->handle_list_jobs(cmd);
    });
    register_command_handler("push_script", [this](const Command& cmd) {
        return manager_->handle_push_script(cmd);
    });
    register_command_handler("list_scripts", [this](const Command& cmd) {
        return manager_->handle_list_scripts(cmd);
    });
    register_command_handler("delete_script", [this](const Command& cmd) {
        return manager_->handle_delete_script(cmd);
    });
}

AgentHttpServer::~AgentHttpServer() {
    stop();
}

void AgentHttpServer::start() {
    if (running_) return;
    
    running_ = true;
    server_thread_ = std::thread(&AgentHttpServer::server_loop, this);
            
}

void AgentHttpServer::stop() {
    if (!running_) return;
    
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
            
}

void AgentHttpServer::register_command_handler(const std::string& command, CommandHandler handler) {
    command_handlers_[command] = handler;
}

void AgentHttpServer::server_loop() {
    // Простая реализация HTTP сервера через TCP сокеты
            
    
#ifdef _WIN32
    // Устанавливаем кодировку UTF-8 для корректной работы с русским текстом
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        
        return;
    }
#endif

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        
        return;
    }

    // Устанавливаем опцию переиспользования адреса
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config_.command_server_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        
        return;
    }

    if (listen(server_socket, 5) < 0) {
        
        return;
    }

            

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running_) {
                
            }
            continue;
        }

        // Обрабатываем запрос в отдельном потоке или синхронно
        std::thread([this, client_socket]() {
            try {
                this->handle_client_request(client_socket);
            } catch (const std::exception& e) {
                std::cerr << "Error handling client request: " << e.what() << std::endl;
                // Отправляем ошибку клиенту
                std::string error_response = generate_response(500, "application/json", 
                    "{\"success\": false, \"message\": \"Internal server error\"}");
                send(client_socket, error_response.c_str(), error_response.length(), 0);
            } catch (...) {
                std::cerr << "Unknown error handling client request" << std::endl;
                // Отправляем ошибку клиенту
                std::string error_response = generate_response(500, "application/json", 
                    "{\"success\": false, \"message\": \"Unknown internal error\"}");
                send(client_socket, error_response.c_str(), error_response.length(), 0);
            }
        }).detach();
    }

#ifdef _WIN32
    closesocket(server_socket);
    WSACleanup();
#else
    close(server_socket);
#endif
}

void AgentHttpServer::handle_client_request(int client_socket) {
    std::string request;
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        request.append(buffer, buffer + bytes_received);
        
        // Try to read remaining bytes if Content-Length present
        auto cl_pos = request.find("Content-Length:");
        size_t content_length = 0;
        if (cl_pos != std::string::npos) {
            auto endline = request.find("\r\n", cl_pos);
            if (endline != std::string::npos) {
                std::string header_line = request.substr(cl_pos, endline - cl_pos);
                size_t colon = header_line.find(":");
                if (colon != std::string::npos) {
                    try { 
                        std::string length_str = header_line.substr(colon + 1);
                        // Убираем пробелы
                        length_str.erase(0, length_str.find_first_not_of(" \t"));
                        content_length = static_cast<size_t>(std::stoul(length_str)); 
                    } catch (...) {}
                }
            }
        }
        
        // Читаем тело запроса полностью
        size_t json_start = request.find("\r\n\r\n");
        if (json_start != std::string::npos) {
            size_t have = request.size() - (json_start + 4);
            while (content_length > 0 && have < content_length) {
                bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes_received <= 0) break;
                request.append(buffer, buffer + bytes_received);
                have += bytes_received;
            }
        }

        std::string response;
        
        
        if (request.find("POST /command") != std::string::npos) {
            size_t body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                std::string json_data = request.substr(body_pos + 4);
                
                
                // Проверяем корректность UTF-8
                if (!is_valid_utf8(json_data)) {
                    
                    
                    response = generate_response(400, "application/json", 
                        "{\"success\": false, \"message\": \"Invalid UTF-8 encoding in request\"}");
                } else {
                    
                    CommandResponse cmd_response = handle_command_request(json_data);
                    response = generate_response(200, "application/json", cmd_response.to_json().dump());
                }
            } else {
                response = generate_response(400, "application/json", "{\"success\": false, \"message\": \"No JSON data found\"}");
            }
        } else {
            response = generate_response(404, "application/json", "{\"success\": false, \"message\": \"Endpoint not found\"}");
        }
        send(client_socket, response.c_str(), response.length(), 0);
    }
    
#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

CommandResponse AgentHttpServer::handle_command_request(const std::string& json_data) {
    try {
        // Дополнительная проверка UTF-8
        if (!is_valid_utf8(json_data)) {
            // Пытаемся очистить строку от проблемных символов
            std::string cleaned_data = clean_utf8(json_data);
            if (!is_valid_utf8(cleaned_data)) {
                return CommandResponse{false, "Invalid UTF-8 encoding in request (could not clean)", {}, current_iso_time()};
            }
            // Используем очищенную строку
            return process_cleaned_json_request(cleaned_data);
        }
        
        // Оригинальная строка валидна, обрабатываем её
        return process_cleaned_json_request(json_data);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing request: " << e.what() << std::endl;
        return CommandResponse{false, "Error parsing request: " + std::string(e.what()), {}, current_iso_time()};
    } catch (...) {
        std::cerr << "Unknown error parsing request" << std::endl;
        return CommandResponse{false, "Unknown error parsing request", {}, current_iso_time()};
    }
}

// Вспомогательная функция для обработки очищенного JSON
CommandResponse AgentHttpServer::process_cleaned_json_request(const std::string& json_data) {
    try {
        // Парсим JSON запрос
        nlohmann::json request_json = nlohmann::json::parse(json_data);
        Command cmd = Command::from_json(request_json);
        
        // Ищем обработчик
        auto it = command_handlers_.find(cmd.command);
        if (it != command_handlers_.end()) {
            try {
                // Дополнительная защита от исключений в обработчике команд
                CommandResponse resp = it->second(cmd);
                return resp;
            } catch (const std::exception& e) {
                // Логируем ошибку и возвращаем безопасный ответ
                std::cerr << "Error in command handler '" << cmd.command << "': " << e.what() << std::endl;
                return CommandResponse{false, "Internal error in command handler: " + std::string(e.what()), {}, current_iso_time()};
            } catch (...) {
                // Защита от неизвестных исключений
                std::cerr << "Unknown error in command handler '" << cmd.command << "'" << std::endl;
                return CommandResponse{false, "Unknown internal error in command handler", {}, current_iso_time()};
            }
        } else {
            return CommandResponse{false, "Unknown command: " + cmd.command, {}, current_iso_time()};
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing cleaned JSON: " << e.what() << std::endl;
        return CommandResponse{false, "Error processing JSON: " + std::string(e.what()), {}, current_iso_time()};
    } catch (...) {
        std::cerr << "Unknown error processing cleaned JSON" << std::endl;
        return CommandResponse{false, "Unknown error processing JSON", {}, current_iso_time()};
    }
}


std::string AgentHttpServer::generate_response(int status_code, const std::string& content_type, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " OK\r\n";
    oss << "Content-Type: " << content_type << "; charset=utf-8\r\n";
    oss << "Content-Length: " << body.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

// MonitoringServerClient implementation
MonitoringServerClient::MonitoringServerClient(const AgentConfig& config) : config_(config) {
    // Автоматически определяем ID и имя, если не заданы
    config_.auto_detect_agent_info();
    
    agent_id_ = config_.agent_id;
    machine_name_ = config_.machine_name;
    
            
}

bool MonitoringServerClient::send_metrics(const nlohmann::json& metrics) {
    try {
        // Добавляем информацию об агенте
        nlohmann::json data = metrics;
        data["agent_id"] = agent_id_;
        data["machine_name"] = machine_name_;
        
        // Проверяем корректность JSON перед отправкой
        std::string json_str = data.dump();
        
        
        nlohmann::json response;
        bool success = make_request("/metrics", data, response);
        
        if (success) {
            
        } else {
            
        }
        
        return success;
    } catch (const std::exception& e) {
        
        return false;
    }
}

bool MonitoringServerClient::register_agent() {
    try {
        // Agent registration happens automatically when sending metrics
        // The server creates agent records when receiving metrics
        
        return true;
    } catch (const std::exception& e) {
        
        return false;
    }
}

bool MonitoringServerClient::update_config_from_server() {
    try {
        nlohmann::json response;
        if (make_request("/api/agents/" + agent_id_ + "/config", {}, response)) {
            // Обновляем конфигурацию
            config_.update_from_json(response);
            
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        
        return false;
    }
}

bool MonitoringServerClient::make_request(const std::string& endpoint, const nlohmann::json& data, nlohmann::json& response) {
    try {
        
        
        std::string url;
        // Всегда используем базовый URL + endpoint
        url = config_.server_url + endpoint;
                    
        
        // Проверяем корректность JSON данных
        std::string json_body;
        try {
            json_body = data.dump();
                    
        } catch (const std::exception& e) {
            
            return false;
        }
        
        auto cpr_response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json; charset=utf-8"}},
            cpr::Body{json_body},
            cpr::Timeout{config_.send_timeout_ms}
        );
        
        if (cpr_response.status_code == 200) {
            if (!cpr_response.text.empty()) {
                try {
                    response = nlohmann::json::parse(cpr_response.text);
                } catch (const std::exception& e) {
                                    
                }
            }
            return true;
        } else {
            
            return false;
        }
    } catch (const std::exception& e) {
        
        return false;
    }
}

// AgentManager implementation
AgentManager::AgentManager(const AgentConfig& config, const std::string& config_path) : config_(config), config_path_(config_path) {
    initialize_metrics_collector();
    http_server_ = std::make_unique<AgentHttpServer>(config_, this);
    server_client_ = std::make_unique<MonitoringServerClient>(config_);
}

AgentManager::~AgentManager() {
    stop();
}

void AgentManager::start() {
    if (running_) return;
    
    running_ = true;
    
    // Запускаем HTTP сервер
    http_server_->start();
    
    // Регистрируемся на сервере
    server_client_->register_agent();
    
    // Запускаем потоки
    metrics_thread_ = std::thread(&AgentManager::metrics_loop, this);
    
    
}

void AgentManager::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Останавливаем HTTP сервер
    if (http_server_) {
        http_server_->stop();
    }
    
    // Ждем завершения потоков
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    
    
}

CommandResponse AgentManager::handle_collect_metrics(const Command& cmd) {
    try {
        std::vector<std::string> requested_metrics;
        if (cmd.data.contains("metrics")) {
            // Поддерживаем как старый формат (массив строк), так и новый (объект с флагами)
            if (cmd.data["metrics"].is_array()) {
                requested_metrics = cmd.data["metrics"].get<std::vector<std::string>>();
            } else if (cmd.data["metrics"].is_object()) {
                // Новый формат: объект с флагами
                for (auto it = cmd.data["metrics"].begin(); it != cmd.data["metrics"].end(); ++it) {
                    if (it.value().get<bool>()) {
                        requested_metrics.push_back(it.key());
                    }
                }
            }
        }
        
        auto metrics = collect_metrics(requested_metrics);
        server_client_->send_metrics(metrics);
        
        return CommandResponse{true, "Metrics collected and sent", metrics, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error collecting metrics: " + std::string(e.what()), {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_update_config(const Command& cmd) {
    try {
        config_.update_from_json(cmd.data);
        
        // Используем сохраненный путь к конфигурационному файлу
        if (!config_path_.empty()) {
            config_.save_to_file(config_path_);
            
        } else {
            config_.save_to_file();
            
        }
        
        
        
        return CommandResponse{true, "Configuration updated", config_.to_json(), current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error updating config: " + std::string(e.what()), {}, current_iso_time()};
    }
}

CommandResponse AgentManager::handle_restart(const Command& cmd) {
    // В реальной реализации здесь будет перезапуск агента
    return CommandResponse{true, "Restart command received", {}, current_iso_time()};
}

CommandResponse AgentManager::handle_stop(const Command& cmd) {
    // Останавливаем агент
    stop();
    return CommandResponse{true, "Stop command received", {}, current_iso_time()};
}

CommandResponse AgentManager::handle_run_script(const Command& cmd) {
    try {
        // Extract options
        std::string interpreter = "auto";
        std::string script;
        std::string script_path;
        std::vector<std::string> args;
        std::unordered_map<std::string, std::string> env;
        std::string working_dir;
        int timeout_sec = config_.max_script_timeout_sec;
        bool capture_output = true;
        // UserParameter-like
        std::string key;
        std::vector<std::string> key_params;

        if (cmd.data.contains("interpreter")) interpreter = cmd.data["interpreter"].get<std::string>();
        if (cmd.data.contains("script") && cmd.data["script"].is_string()) script = cmd.data["script"].get<std::string>();
        if (cmd.data.contains("script_path") && cmd.data["script_path"].is_string()) script_path = cmd.data["script_path"].get<std::string>();
        if (cmd.data.contains("args") && cmd.data["args"].is_array()) {
            for (const auto& a : cmd.data["args"]) args.push_back(a.get<std::string>());
        }
        if (cmd.data.contains("env") && cmd.data["env"].is_object()) {
            for (auto it = cmd.data["env"].begin(); it != cmd.data["env"].end(); ++it) env[it.key()] = it.value().get<std::string>();
        }
        if (cmd.data.contains("working_dir")) working_dir = cmd.data["working_dir"].get<std::string>();
        if (cmd.data.contains("timeout_sec")) timeout_sec = std::min<int>(cmd.data["timeout_sec"].get<int>(), config_.max_script_timeout_sec);
        if (cmd.data.contains("capture_output")) capture_output = cmd.data["capture_output"].get<bool>();
        if (cmd.data.contains("key") && cmd.data["key"].is_string()) key = cmd.data["key"].get<std::string>();
        if (cmd.data.contains("params") && cmd.data["params"].is_array()) {
            for (const auto& p : cmd.data["params"]) key_params.push_back(p.get<std::string>());
        }

        // Resolve UserParameter mapping
        if (!key.empty()) {
            auto it = config_.user_parameters.find(key);
            if (it == config_.user_parameters.end()) {
                // try wildcard form key[*]
                auto itw = config_.user_parameters.find(key + "[*]");
                if (itw == config_.user_parameters.end()) {
                    return CommandResponse{false, "Unknown user parameter key: " + key, {}, current_iso_time()};
                }
                std::string templ = itw->second;
                std::string resolved = substitute_params(templ, key_params);
                script = resolved;
                // allow inline for user parameters regardless of flag
            } else {
                script = substitute_params(it->second, key_params);
            }
            interpreter = "auto"; // pick based on platform/heuristics
        }

        // Validate interpreter
        auto pick_interpreter = [&](const std::string& path_or_empty) -> std::string {
            if (interpreter != "auto") return interpreter;
#ifdef _WIN32
            // Detect by extension
            if (!path_or_empty.empty()) {
                std::filesystem::path p(path_or_empty);
                auto ext = p.extension().string();
                if (ext == ".ps1") return "powershell";
                if (ext == ".py") return "python";
                if (ext == ".bat" || ext == ".cmd") return "cmd";
            }
            
            // For inline scripts, try to detect PowerShell commands
            if (!script.empty()) {
                // Check if script contains PowerShell-specific commands
                std::string script_lower = script;
                std::transform(script_lower.begin(), script_lower.end(), script_lower.begin(), ::tolower);
                if (script_lower.find("write-host") != std::string::npos ||
                    script_lower.find("write-output") != std::string::npos ||
                    script_lower.find("write-error") != std::string::npos ||
                    script_lower.find("get-process") != std::string::npos ||
                    script_lower.find("get-service") != std::string::npos ||
                    script_lower.find("$") != std::string::npos) {
                    
                    return "powershell";
                }
            }
            
            return "cmd";
#else
            if (!path_or_empty.empty()) {
                std::filesystem::path p(path_or_empty);
                auto ext = p.extension().string();
                if (ext == ".sh") return "bash";
                if (ext == ".py") return "python";
            }
            return "bash";
#endif
        };

        // If using script_path, ensure within scripts_dir
        if (!script_path.empty()) {
            std::filesystem::path base = AgentConfig::get_scripts_path(config_.scripts_dir);
            std::filesystem::path target = base / script_path; // Создаем полный путь
            
            // Проверяем, что целевой путь находится внутри базовой папки scripts
            if (!is_subpath(base, target)) {
                return CommandResponse{false, "script_path is outside scripts_dir", {}, current_iso_time()};
            }
        } else if (script.empty()) {
            return CommandResponse{false, "Either script or script_path must be provided", {}, current_iso_time()};
        } else {
            // inline script allowed?
            if (!config_.enable_inline_commands && key.empty()) {
                return CommandResponse{false, "Inline scripts are disabled by configuration", {}, current_iso_time()};
            }
        }

        std::string chosen = pick_interpreter(script_path);
        if (!is_allowed_interpreter(config_.allowed_interpreters, chosen)) {
            return CommandResponse{false, "Interpreter is not allowed: " + chosen, {}, current_iso_time()};
        }

        // Build argv per platform/interpreter
        std::vector<std::string> argv;
#ifdef _WIN32
        if (chosen == "powershell") {
            argv = {"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command"};
            if (!script.empty()) {
                // Для inline скриптов добавляем команду смены кодировки
                std::string full_script = "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; chcp 65001 >nul; " + script;
                argv.push_back(full_script);
            } else {
                // Для файлов добавляем команду смены кодировки перед выполнением
                std::filesystem::path scripts_base = AgentConfig::get_scripts_path(config_.scripts_dir);
                std::filesystem::path full_script_path = scripts_base / script_path;
                std::string full_script = "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; chcp 65001 >nul; & '" + full_script_path.string() + "'";
                for (const auto& a : args) full_script += " " + a;
                argv.push_back(full_script);
            }
        } else if (chosen == "cmd") {
            argv = {"cmd.exe", "/c"};
            std::ostringstream cmdline;
            // Добавляем команду смены кодировки для CMD
            cmdline << "chcp 65001 >nul && ";
            if (!script.empty()) cmdline << script;
            else {
                // Создаем полный путь к файлу для CMD
                std::filesystem::path scripts_base = AgentConfig::get_scripts_path(config_.scripts_dir);
                std::filesystem::path full_script_path = scripts_base / script_path;
                cmdline << '"' << full_script_path.string() << '"';
                for (const auto& a : args) { cmdline << ' ' << a; }
            }
            argv.push_back(cmdline.str());
        } else if (chosen == "python") {
            // На Windows пробуем разные варианты Python
#ifdef _WIN32
            std::string python_cmd = "python";
            // Проверяем доступность python.exe
            if (system("python --version >nul 2>&1") != 0) {
                // Пробуем python3
                if (system("python3 --version >nul 2>&1") == 0) {
                    python_cmd = "python3";
                } else if (system("py --version >nul 2>&1") == 0) {
                    python_cmd = "py";
                } else {
                    // Пробуем найти Python в стандартных путях
                    std::vector<std::string> python_paths = {
                        "C:\\Python311\\python.exe",
                        "C:\\Python310\\python.exe", 
                        "C:\\Python39\\python.exe",
                        "C:\\Users\\rosih\\AppData\\Local\\Programs\\Python\\Launcher\\py.exe",
                        "C:\\Users\\rosih\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
                        "C:\\Users\\rosih\\AppData\\Local\\Programs\\Python\\Python310\\python.exe",
                        "C:\\Users\\rosih\\AppData\\Local\\Programs\\Python\\Python39\\python.exe"
                    };
                    
                    // Также пробуем найти py.exe в PATH через where
                    std::string where_cmd = "where py >nul 2>&1";
                    if (system(where_cmd.c_str()) == 0) {
                        python_cmd = "py";
                    } else {
                        bool found = false;
                        for (const auto& path : python_paths) {
                            if (std::filesystem::exists(path)) {
                                python_cmd = path;
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            return CommandResponse{false, "Python not found. Please install Python and add it to PATH", {}, current_iso_time()};
                        }
                    }
                }
            }
            argv = {python_cmd};
            // Логируем выбранную команду Python
            append_audit(config_, std::string("PYTHON_CMD: ") + python_cmd);
#else
            argv = {"python3"};
#endif
            if (!script.empty()) { 
                // Выполняем inline скрипт Python
                argv.push_back("-c"); 
                argv.push_back(script); 
            }
            else { 
                std::filesystem::path scripts_base = AgentConfig::get_scripts_path(config_.scripts_dir);
                std::filesystem::path full_script_path = scripts_base / script_path;
                argv.push_back(full_script_path.string()); 
                for (const auto& a : args) argv.push_back(a); 
            }
        } else {
            return CommandResponse{false, "Unsupported interpreter on Windows: " + chosen, {}, current_iso_time()};
        }
#else
        if (chosen == "bash") {
            if (!script.empty()) {
                argv = {"/bin/bash", "-lc", script};
            } else {
                std::filesystem::path scripts_base = AgentConfig::get_scripts_path(config_.scripts_dir);
                std::filesystem::path full_script_path = scripts_base / script_path;
                argv = {"/bin/bash", full_script_path.string()};
                for (const auto& a : args) argv.push_back(a);
            }
        } else if (chosen == "python") {
            if (!script.empty()) { argv = {"python3", "-c", script}; }
            else { 
                std::filesystem::path scripts_base = AgentConfig::get_scripts_path(config_.scripts_dir);
                std::filesystem::path full_script_path = scripts_base / script_path;
                argv = {"python3", full_script_path.string()}; 
                for (const auto& a : args) argv.push_back(a); 
            }
        } else {
            return CommandResponse{false, "Unsupported interpreter on POSIX: " + chosen, {}, current_iso_time()};
        }
#endif

        auto start = std::chrono::steady_clock::now();
        std::shared_ptr<BackgroundJobInfo> bg_ref;
#ifdef _WIN32
        auto exec_callable = [this, argv, env, working_dir, timeout_sec, &bg_ref]() {
            auto cancelled = [&bg_ref]() -> bool { return bg_ref && bg_ref->cancel_requested.load(); };
            return run_process_windows(argv, env, working_dir, timeout_sec, config_.max_output_bytes, cancelled);
        };
#else
        auto exec_callable = [this, argv, env, working_dir, timeout_sec, &bg_ref]() {
            auto cancelled = [&bg_ref]() -> bool { return bg_ref && bg_ref->cancel_requested.load(); };
            return run_process_posix(argv, env, working_dir, timeout_sec, config_.max_output_bytes, cancelled);
        };
#endif

        if (cmd.data.contains("background") && cmd.data["background"].get<bool>()) {
            // enforce max concurrent jobs
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                size_t active = 0;
                for (const auto& [_, j] : jobs_) if (!j->completed.load()) ++active;
                if (active >= static_cast<size_t>(config_.max_concurrent_jobs)) {
                    return CommandResponse{false, "Too many concurrent jobs", {}, current_iso_time()};
                }
            }
            auto job = std::make_shared<BackgroundJobInfo>();
            job->job_id = generate_job_id();
            bg_ref = job;
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                jobs_[job->job_id] = job;
            }
            std::thread([this, job, exec_callable]() {
                auto t0 = std::chrono::steady_clock::now();
                job->started_at_sec = std::chrono::duration_cast<std::chrono::seconds>(t0.time_since_epoch()).count();
                ProcessResult pr = exec_callable();
                auto t1 = std::chrono::steady_clock::now();
                job->duration_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                job->completed_at_sec = std::chrono::duration_cast<std::chrono::seconds>(t1.time_since_epoch()).count();
                job->timed_out = pr.timed_out;
                job->exit_code = pr.exit_code;
                job->truncated = pr.truncated;
                job->output = std::move(pr.combined_output);
                job->completed = true;
                append_audit(config_, std::string("JOB_COMPLETE id=") + job->job_id + " exit=" + std::to_string(job->exit_code.load()));
            }).detach();
            append_audit(config_, std::string("JOB_START id=") + job->job_id);
            nlohmann::json jd;
            jd["job_id"] = job->job_id;
            return CommandResponse{true, "Job started", jd, current_iso_time()};
        }

        ProcessResult pr = exec_callable();
        auto end = std::chrono::steady_clock::now();
        auto dur_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

        nlohmann::json data;
        data["exit_code"] = pr.exit_code;
        data["stdout"] = capture_output ? pr.stdout_output : "";
        data["stderr"] = capture_output ? pr.stderr_output : "";
        data["combined_output"] = capture_output ? pr.combined_output : "";
        data["duration_ms"] = dur_ms;
        data["truncated"] = pr.truncated;
        if (pr.timed_out) {
            append_audit(config_, "RUN_SCRIPT timeout");
            return CommandResponse{false, "Process timed out", data, current_iso_time()};
        }
        bool success = (pr.exit_code == 0);
        append_audit(config_, std::string("RUN_SCRIPT exit=") + std::to_string(pr.exit_code));
        return CommandResponse{success, success ? "Exited with code 0" : (std::string("Exited with code ") + std::to_string(pr.exit_code)), data, current_iso_time()};
    } catch (const std::exception& e) {
        return CommandResponse{false, std::string("Error running script: ") + e.what(), {}, current_iso_time()};
    }
}
void AgentManager::purge_old_jobs() {
    const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        const auto& job = it->second;
        if (job->completed.load() && job->completed_at_sec > 0 && (now_sec - job->completed_at_sec) > config_.job_retention_seconds) {
            it = jobs_.erase(it);
        } else {
            ++it;
        }
    }
}

nlohmann::json AgentManager::collect_metrics(const std::vector<std::string>& requested_metrics) {
    if (!metrics_collector_) {
        throw std::runtime_error("Metrics collector not initialized");
    }
    
    auto metrics = metrics_collector_->collect();
    
    // Определяем, какие метрики собирать
    std::vector<std::string> enabled_metrics = requested_metrics.empty() ? 
        config_.get_enabled_metrics_list() : requested_metrics;
    
    // Конвертируем в JSON
    nlohmann::json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(metrics.timestamp.time_since_epoch()).count();
    j["machine_type"] = metrics.machine_type;
    j["agent_id"] = config_.agent_id;
    j["machine_name"] = config_.machine_name;
    
    // Добавляем конфигурацию агента
    j["config"] = config_.to_json();
    
    // Добавляем метрики согласно enabled_metrics (убираем проверку config_.is_metric_enabled)
    for (const auto& metric_type : enabled_metrics) {
        if (metric_type == "cpu") {
            j["cpu"]["usage_percent"] = metrics.cpu.usage_percent;
            j["cpu"]["temperature"] = metrics.cpu.temperature;
            j["cpu"]["core_temperatures"] = metrics.cpu.core_temperatures;
            j["cpu"]["core_usage"] = metrics.cpu.core_usage;
        } else if (metric_type == "memory") {
            j["memory"]["total_bytes"] = metrics.memory.total_bytes;
            j["memory"]["used_bytes"] = metrics.memory.used_bytes;
            j["memory"]["free_bytes"] = metrics.memory.free_bytes;
            j["memory"]["usage_percent"] = metrics.memory.usage_percent;
        } else if (metric_type == "disk") {
            j["disk"]["partitions"] = nlohmann::json::array();
            for (const auto& part : metrics.disk.partitions) {
                nlohmann::json jp;
                jp["mount_point"] = part.mount_point;
                jp["filesystem"] = part.filesystem;
                jp["total_bytes"] = static_cast<int64_t>(part.total_bytes);
                jp["used_bytes"] = static_cast<int64_t>(part.used_bytes);
                jp["free_bytes"] = static_cast<int64_t>(part.free_bytes);
                if (part.usage_percent >= 0) {
                    jp["usage_percent"] = static_cast<double>(part.usage_percent);
                }
                j["disk"]["partitions"].push_back(jp);
            }
        } else if (metric_type == "network") {
            j["network"]["interfaces"] = nlohmann::json::array();
            for (const auto& iface : metrics.network.interfaces) {
                nlohmann::json ji;
                ji["name"] = iface.name;
                ji["bytes_sent"] = static_cast<int64_t>(iface.bytes_sent);
                ji["bytes_received"] = static_cast<int64_t>(iface.bytes_received);
                ji["packets_sent"] = static_cast<int64_t>(iface.packets_sent);
                ji["packets_received"] = static_cast<int64_t>(iface.packets_received);
                if (iface.bandwidth_sent >= 0) {
                    ji["bandwidth_sent"] = static_cast<double>(iface.bandwidth_sent);
                }
                if (iface.bandwidth_received >= 0) {
                    ji["bandwidth_received"] = static_cast<double>(iface.bandwidth_received);
                }
                j["network"]["interfaces"].push_back(ji);
            }
            
            // Добавляем сериализацию connections
            j["network"]["connections"] = nlohmann::json::array();
            for (const auto& conn : metrics.network.connections) {
                nlohmann::json jc;
                jc["local_ip"] = conn.local_ip;
                jc["local_port"] = conn.local_port;
                jc["remote_ip"] = conn.remote_ip;
                jc["remote_port"] = conn.remote_port;
                jc["protocol"] = conn.protocol;
                j["network"]["connections"].push_back(jc);
            }
        } else if (metric_type == "gpu") {
            j["gpu"]["temperature"] = metrics.gpu.temperature;
            j["gpu"]["usage_percent"] = metrics.gpu.usage_percent;
            j["gpu"]["memory_used"] = metrics.gpu.memory_used;
            j["gpu"]["memory_total"] = metrics.gpu.memory_total;
        } else if (metric_type == "hdd") {
            // HDD метрики (если доступны)
            j["hdd"]["drives"] = nlohmann::json::array();
            // TODO: Добавить сбор HDD метрик, когда они будут реализованы в MetricsCollector
        } else if (metric_type == "user") {
            j["user"]["username"] = metrics.user.username;
            j["user"]["domain"] = metrics.user.domain;
            j["user"]["full_name"] = metrics.user.full_name;
            j["user"]["user_sid"] = metrics.user.user_sid;
            j["user"]["is_active"] = metrics.user.is_active;
        } else if (metric_type == "inventory") {
            j["inventory"]["device_type"] = metrics.inventory.device_type;
            j["inventory"]["manufacturer"] = metrics.inventory.manufacturer;
            j["inventory"]["model"] = metrics.inventory.model;
            j["inventory"]["serial_number"] = metrics.inventory.serial_number;
            j["inventory"]["uuid"] = metrics.inventory.uuid;
            j["inventory"]["os_name"] = metrics.inventory.os_name;
            j["inventory"]["os_version"] = metrics.inventory.os_version;
            j["inventory"]["cpu_model"] = metrics.inventory.cpu_model;
            j["inventory"]["cpu_frequency"] = metrics.inventory.cpu_frequency;
            j["inventory"]["memory_type"] = metrics.inventory.memory_type;
            j["inventory"]["disk_model"] = metrics.inventory.disk_model;
            j["inventory"]["disk_type"] = metrics.inventory.disk_type;
            j["inventory"]["disk_total_bytes"] = metrics.inventory.disk_total_bytes;
            j["inventory"]["gpu_model"] = metrics.inventory.gpu_model;
            j["inventory"]["mac_addresses"] = metrics.inventory.mac_addresses;
            j["inventory"]["ip_addresses"] = metrics.inventory.ip_addresses;
            j["inventory"]["installed_software"] = metrics.inventory.installed_software;
        }
    }
    
    return j;
}

void AgentManager::metrics_loop() {
    while (running_) {
        try {
            auto metrics = collect_metrics();
            server_client_->send_metrics(metrics);
            // periodic purge of old jobs
            purge_old_jobs();
        } catch (const std::exception& e) {
            
        }
        
        // Используем update_frequency как интервал сбора метрик
        std::this_thread::sleep_for(std::chrono::seconds(config_.update_frequency));
    }
}

void AgentManager::initialize_metrics_collector() {
#ifdef _WIN32
    metrics_collector_ = std::make_unique<monitoring::WindowsMetricsCollector>();
#else
    metrics_collector_ = monitoring::create_metrics_collector();
#endif
}

} // namespace agent 