#include <iostream>
#include <deque>
#include <set>
#include <boost/asio.hpp>

using namespace boost::asio;
using ip::tcp;

class FixSession : public std::enable_shared_from_this<FixSession> {
public:
    FixSession(tcp::socket socket, io_service& ioService)
        : socket_(std::move(socket)), ioService_(ioService) {
    }

    void start() {
        doRead();
    }

    void sendMessage(const std::string& message) {
        ioService_.post([this, message]() {
            bool writeInProgress = !writeQueue_.empty();
            writeQueue_.push_back(message);

            if (!writeInProgress) {
                doWrite();
            }
        });
    }

private:
    void doRead() {
        auto self(shared_from_this());
        async_read_until(socket_, inputBuffer_, '\n',
            [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (!ec) {
                    handleRead();
                    doRead();  // Continue reading
                } else {
                    // Handle error or connection closure
                    // e.g., close the session
                }
            });
    }

    void handleRead() {
        // Implement your logic to process received FIX messages
        // e.g., parse the message, handle different FIX tags, etc.
        std::istream is(&inputBuffer_);
        std::string receivedMessage;
        std::getline(is, receivedMessage);

        // Process the received FIX message
        std::cout << "Received FIX message: " << receivedMessage << std::endl;
    }

    void doWrite() {
        auto self(shared_from_this());
        async_write(socket_, buffer(writeQueue_.front() + '\n'),
            [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (!ec) {
                    writeQueue_.pop_front();

                    if (!writeQueue_.empty()) {
                        doWrite();  // Continue writing
                    }
                } else {
                    // Handle error or connection closure
                    // e.g., close the session
                }
            });
    }

    tcp::socket socket_;
    io_service& ioService_;
    boost::asio::streambuf inputBuffer_;
    std::deque<std::string> writeQueue_;
};

class Server {
public:
    Server(io_service& ioService, short port)
        : acceptor_(ioService, tcp::endpoint(tcp::v4(), port)),
          socket_(ioService), ioService_(ioService) {
        startAccept();
    }

private:
    void startAccept() {
        acceptor_.async_accept(socket_, [this, &ioService = ioService_](boost::system::error_code ec) {
            if (!ec) {
                std::cout << "Client connected: " << socket_.remote_endpoint() << std::endl;
                auto session = std::make_shared<FixSession>(std::move(socket_), ioService);
                sessions_.insert(session);
                session->start();
            }

            startAccept();  // Continue to accept new connections
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    io_service& ioService_;
    std::set<std::shared_ptr<FixSession>> sessions_;
};

class Client {
public:
    Client(io_service& ioService, const std::string& serverAddress, short port)
        : socket_(ioService), ioService_(ioService) {
        tcp::resolver resolver(ioService);
        auto endpoint_iterator = resolver.resolve({serverAddress, std::to_string(port)});
        connect(socket_, endpoint_iterator);

        auto session = std::make_shared<FixSession>(std::move(socket_), ioService);
        session->start();
    }

private:
    tcp::socket socket_;
    io_service& ioService_;
};

int main() {
    try {
        boost::asio::io_service ioService;

        // Start the server
        Server server(ioService, 1234);

        // Connect multiple clients
        Client client1(ioService, "127.0.0.1", 1234);
        Client client2(ioService, "127.0.0.1", 1234);

        // Perform any additional tasks or wait for user input if needed

        ioService.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
