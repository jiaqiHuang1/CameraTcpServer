#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>

using boost::asio::ip::tcp;

// Get local IPv4 address
std::string get_local_ipv4_address() {
    try {
        boost::asio::io_context io_context;
        // Create a resolver to get all IP addresses corresponding to the local hostname
        tcp::resolver resolver(io_context);
        tcp::resolver::query query(boost::asio::ip::host_name(), "");
        auto endpoints = resolver.resolve(query);

        // Iterate through the parsing results to find the first IPv4 address
        for (auto iter = endpoints.begin(); iter != endpoints.end(); ++iter) {
            auto endpoint = iter->endpoint();
            auto address = endpoint.address();
            // Takes only IPv4 addresses
            if (address.is_v4()) {                 
                return address.to_string();
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error getting local IP address: " << e.what() << std::endl;
    }
    return "falsh"; // Return Error
}

// Global Camera Example
cv::VideoCapture capture;

// Takes a picture and sends the image data to the client
void takePhotoAndSend(tcp::socket& socket) {
    if (!capture.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        return;
    }

    cv::Mat frame;
    capture >> frame; // Taking a frame
    if (frame.empty()) {
        std::cerr << "Error: Captured empty frame." << std::endl;
        return;
    }

    // Encoding images into JPEG format and storing them in memory
    std::vector<uchar> buffer;
    cv::imencode(".jpg", frame, buffer);

    // Get image size and send size information
    int imageSize = buffer.size();
    boost::asio::write(socket, boost::asio::buffer(&imageSize, sizeof(imageSize)));

    // Send image data
    boost::asio::write(socket, boost::asio::buffer(buffer.data(), buffer.size()));
    std::cout << "Photo sent to client." << std::endl;
}

// Handling Single Client Connections
void handle_client(tcp::socket socket) {
    try {
        std::cout << "Client connected!" << std::endl;

        // Processing client requests
        while (true) {
            char request[1024];
            boost::system::error_code error;
            size_t length = socket.read_some(boost::asio::buffer(request), error);

            if (error == boost::asio::error::eof) {
                std::cout << "Client disconnected." << std::endl;
                break; // Connection is closed, jumping out of the loop
            }
            else if (error) {
                std::cerr << "Read error: " << error.message() << std::endl;
                break;
            }

            std::string requestStr(request, length);

            // If the ‘TAKE_PHOTO’ command is received, take a picture and send it
            if (requestStr == "TAKE_PHOTO") {
                std::cout << "Received TAKE_PHOTO request." << std::endl;
                takePhotoAndSend(socket);
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception in client handler: " << e.what() << std::endl;
    }
}

// Starting a multi-threaded TCP server
void start_server(boost::asio::io_context& io_context, unsigned short port) {
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Server started. Waiting for connections on port " << port << "..." << std::endl;

    while (true) {
        try {
            // Accepting new client connections
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // Create a separate thread for each client connection
            std::thread(handle_client, std::move(socket)).detach();
        }
        catch (std::exception& e) {
            std::cerr << "Exception in acceptor loop: " << e.what() << std::endl;
        }
    }
}

int main() {
    //Try turning on the camera
    capture.open(0);
    if (!capture.isOpened()) {
        std::cerr << "No camera detected or camera cannot be accessed." << std::endl;
        return -1;
    }
    else {
        std::cout << "Camera detected and accessible." << std::endl;
    }

    // Obtain and print the local IPv4 address
    std::string local_ip = get_local_ipv4_address();
    std::cout << "Local IPv4 Address: " << local_ip << std::endl;

    try {
        boost::asio::io_context io_context;
        unsigned short port = 12345;
        start_server(io_context, port);
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    capture.release(); // Release the camera
    return 0;
}
