#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <span>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <array>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/random.h>

namespace servd {

    class RsaKey {
    public:
        RsaKey() = default;
        ~RsaKey() { cleanup_pem_file(); }

        RsaKey(const RsaKey&) = delete;
        RsaKey& operator=(const RsaKey&) = delete;

        RsaKey(RsaKey&& other) noexcept
            : pem_(std::move(other.pem_))
            , pem_path_(std::move(other.pem_path_))
            , pubkey_der_(std::move(other.pubkey_der_))
        {
            other.pem_path_.clear();
        }

        RsaKey& operator=(RsaKey&& other) noexcept {
            if (this != &other) {
                cleanup_pem_file();
                pem_ = std::move(other.pem_);
                pem_path_ = std::move(other.pem_path_);
                pubkey_der_ = std::move(other.pubkey_der_);
                other.pem_path_.clear();
            }
            return *this;
        }

        void generate() {
            FILE* pipe = popen(
                "openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 "
                "-outform PEM 2>/dev/null", "r");
            if (!pipe) throw std::runtime_error("RsaKey: popen(openssl) failed");

            std::array<char, 4096> buf{};
            pem_.clear();
            while (fgets(buf.data(), buf.size(), pipe)) pem_ += buf.data();
            int rc = pclose(pipe);
            if (rc != 0 || pem_.empty())
                throw std::runtime_error("RsaKey: openssl genpkey failed (openssl installed?)");

            char tmpl[] = "/tmp/servd_rsa_XXXXXX";
            int fd = mkstemp(tmpl);
            if (fd < 0) throw std::runtime_error("RsaKey: mkstemp failed");
            ssize_t written = write(fd, pem_.data(), pem_.size());
            close(fd);
            if (written < 0 || static_cast<size_t>(written) != pem_.size()) {
                unlink(tmpl);
                throw std::runtime_error("RsaKey: write pem file failed");
            }
            pem_path_ = tmpl;

            pubkey_der_ = fetch_pubkey_der();
        }

        [[nodiscard]] bool is_ready() const { return !pem_.empty(); }

        [[nodiscard]] std::span<const uint8_t> pubkey_der() const {
            return pubkey_der_;
        }

        // Decrypt a 256-byte RSA-encrypted session key via openssl subprocess.
        // Blocking (fork+exec) but called only once per connection during handshake.
        std::vector<uint8_t> decrypt(std::span<const uint8_t> input) {
            if (!is_ready()) throw std::runtime_error("RsaKey: key not generated");

            int pipe_in[2], pipe_out[2];
            if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
                throw std::runtime_error("RsaKey: pipe failed");

            pid_t pid = fork();
            if (pid < 0) {
                close(pipe_in[0]); close(pipe_in[1]);
                close(pipe_out[0]); close(pipe_out[1]);
                throw std::runtime_error("RsaKey: fork failed");
            }

            if (pid == 0) {
                close(pipe_in[1]);
                close(pipe_out[0]);
                dup2(pipe_in[0], STDIN_FILENO);
                dup2(pipe_out[1], STDOUT_FILENO);
                close(pipe_in[0]);
                close(pipe_out[1]);
                execlp("openssl", "openssl", "pkeyutl", "-decrypt",
                       "-inkey", pem_path_.c_str(),
                       "-pkeyopt", "rsa_padding_mode:pkcs1",
                       nullptr);
                _exit(1);
            }

            close(pipe_in[0]);
            close(pipe_out[1]);

            write(pipe_in[1], input.data(), input.size());
            close(pipe_in[1]);

            std::vector<uint8_t> result(256);
            ssize_t n = read(pipe_out[0], result.data(), result.size());
            close(pipe_out[0]);

            int status = 0;
            waitpid(pid, &status, 0);

            if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                throw std::runtime_error("RsaKey: decrypt failed");

            result.resize(static_cast<size_t>(n));
            return result;
        }

    private:
        std::string pem_;
        std::string pem_path_;
        std::vector<uint8_t> pubkey_der_;

        void cleanup_pem_file() {
            if (!pem_path_.empty()) {
                unlink(pem_path_.c_str());
                pem_path_.clear();
            }
        }

        std::vector<uint8_t> fetch_pubkey_der() {
            char tmpl[] = "/tmp/servd_pub_XXXXXX";
            int fd = mkstemp(tmpl);
            if (fd < 0) throw std::runtime_error("RsaKey: mkstemp for pubkey failed");
            write(fd, pem_.data(), pem_.size());
            close(fd);

            std::string cmd = "openssl pkey -in " + std::string(tmpl) +
                              " -inform PEM -pubout -outform DER 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                unlink(tmpl);
                throw std::runtime_error("RsaKey: popen for pubkey failed");
            }

            std::vector<uint8_t> der;
            std::array<uint8_t, 4096> buf{};
            while (true) {
                size_t n = fread(buf.data(), 1, buf.size(), pipe);
                if (n == 0) break;
                der.insert(der.end(), buf.begin(), buf.begin() + n);
            }
            pclose(pipe);
            unlink(tmpl);

            if (der.empty()) throw std::runtime_error("RsaKey: failed to extract public key DER");
            return der;
        }
    };

}
