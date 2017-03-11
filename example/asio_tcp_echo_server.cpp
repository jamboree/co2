#include <cstdlib>
#include <iostream>
#include <co2/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <act/acceptor.hpp>
#include <act/socket.hpp>
#include <act/write.hpp>

namespace asio = boost::asio;

auto session(asio::ip::tcp::socket sock) CO2_BEG(void, (sock),
    char buf[1024];
    std::size_t len;
    act::error_code ec;
)
{
    CO2_TRY
    {
        std::cout << "connected: " << sock.remote_endpoint() << std::endl;
        for ( ; ; )
        {
            CO2_AWAIT_SET(len, act::read_some(sock, asio::buffer(buf), ec));
            if (ec == asio::error::eof)
                CO2_RETURN();
            CO2_AWAIT(act::write(sock, asio::buffer(buf, len)));
        }
    }
    CO2_CATCH(std::exception& e)
    {
        std::cout << "error: " << sock.remote_endpoint() << ": " << e.what() << std::endl;
    }
} CO2_END

auto server(asio::io_service& io, unsigned short port) CO2_BEG(void, (io, port),
    asio::ip::tcp::endpoint endpoint{asio::ip::tcp::v4(), port};
    asio::ip::tcp::acceptor acceptor{io, endpoint};
    asio::ip::tcp::socket sock{io};
)
{
    std::cout << "server running at: " << endpoint << std::endl;
    for ( ; ; )
    {
        CO2_AWAIT(act::accept(acceptor, sock));
        session(std::move(sock));
    }
} CO2_END

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: asio_tcp_echo_server <port>\n";
        return EXIT_FAILURE;
    }
    asio::io_service io;
    server(io, std::atoi(argv[1]));
    io.run();

    return EXIT_SUCCESS;
}