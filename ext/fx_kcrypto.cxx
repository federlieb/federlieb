#include <linux/if_alg.h>
#include <linux/socket.h>
#include <sys/socket.h>

#include "federlieb/federlieb.hxx"

#include "fx_kcrypto.hxx"

namespace fl = ::federlieb;

auto
make_lambda(const std::string& type,
            const std::string& name,
            size_t digest_bytes)
{
  return [&type, &name, digest_bytes](const fl::blob_type& value) {
    struct sockaddr_alg sa_alg;
    std::memset(&sa_alg, 0, sizeof(sa_alg));

    sa_alg.salg_family = AF_ALG;

    std::memcpy(sa_alg.salg_type, type.c_str(), type.size() + 1);
    std::memcpy(sa_alg.salg_name, name.c_str(), name.size() + 1);

    int sock_fd = ::socket(AF_ALG, SOCK_SEQPACKET, 0);

    fl::error::raise_if(sock_fd < 0, "socket() failed");

    int bind_rc = ::bind(sock_fd, (struct sockaddr*)&sa_alg, sizeof(sa_alg));

    fl::error::raise_if(bind_rc, "bind() failed");

    int fd = ::accept(sock_fd, NULL, 0);

    fl::error::raise_if(fd < 0, "accept() failed");

    ssize_t size = fl::detail::safe_to<ssize_t>(value.size());
    ssize_t write_rc = ::write(fd, &value[0], size);

    fl::error::raise_if(write_rc != size, "write() failed");

    fl::value::blob result;

    fl::blob_type buf(digest_bytes);

    auto read = ::read(fd, &buf[0], buf.size());

    fl::error::raise_if(read < 0, "read() failed");
    fl::error::raise_if(fl::detail::safe_to<size_t>(read) != buf.size(),
                        "read() failed");

    result.value.insert(std::end(result.value), std::begin(buf), std::end(buf));

    ::close(fd);
    ::close(sock_fd);

    return result;
  };
}

fl::value::blob
fx_sha1::xFunc(const fl::blob_type& value)
{
  return make_lambda("hash", "sha1", 20)(value);
}
