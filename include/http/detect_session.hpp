#ifndef DETECT_SESSION_HPP
#define DETECT_SESSION_HPP

#include "http_session.hpp"
#include "ssl_http_session.hpp"
#include "plain_http_session.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

/**
 * @brief The detect_session class is responsible for detecting if an incoming 
 *        TCP connection is an SSL connection or a plain connection. Based on the 
 *        result of the detection, it launches either an SSL session or a plain session.
 */
class detect_session : public std::enable_shared_from_this<detect_session>
{
    beast::tcp_stream stream_;                  ///< The underlying TCP stream for the session.
    ssl::context& ctx_;                         ///< The SSL context, used for configuring SSL sessions.
    std::shared_ptr<std::string const> doc_root_; ///< The root directory for serving HTTP content.
    beast::flat_buffer buffer_;                 ///< Buffer for reading data from the stream.

    public:
    /**
     * @brief Constructor for the detect_session class.
     * 
     * @param socket The TCP socket associated with the incoming connection.
     * @param ctx The SSL context, passed by reference.
     * @param doc_root A shared pointer to the root directory for serving HTTP content.
     */
    detect_session(tcp::socket&& socket, ssl::context& ctx, std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket))  ///< Move the socket into the TCP stream.
          , ctx_(ctx)                   ///< Initialize the SSL context reference.
          , doc_root_(doc_root)         ///< Initialize the document root shared pointer.
    {
    }

    /**
     * @brief Starts the detection process.
     * 
     * This function is responsible for initiating the asynchronous operations
     * that will determine whether the incoming connection should be handled
     * as an SSL connection or a plain connection.
     */
    void run()
    {
        // Ensure that the operations are executed within a strand to prevent race conditions.
        net::dispatch(
                stream_.get_executor(),
                beast::bind_front_handler(
                    &detect_session::on_run,
                    this->shared_from_this()));
    }

    /**
     * @brief Handler function for the run operation.
     * 
     * This function sets a timeout for the operation and then starts the
     * asynchronous SSL detection process.
     */
    void on_run()
    {
        // Set a timeout on the operation.
        stream_.expires_after(std::chrono::seconds(30));

        // Begin asynchronous SSL detection.
        beast::async_detect_ssl(
                stream_,
                buffer_,
                beast::bind_front_handler(
                    &detect_session::on_detect,
                    this->shared_from_this()));
    }

    /**
     * @brief Handler function that is called when the SSL detection completes.
     * 
     * Depending on the result of the detection, this function will launch either
     * an SSL session or a plain session.
     * 
     * @param ec Error code indicating the success or failure of the detection.
     * @param result Boolean result of the detection: true if SSL was detected, false otherwise.
     */
    void on_detect(beast::error_code ec, bool result)
    {
        if(ec)
            return fail(ec, "detect");

        if(result)
        {
            // Launch an SSL session if SSL was detected.
            std::make_shared<ssl_http_session>(
                    std::move(stream_),
                    ctx_,
                    std::move(buffer_),
                    doc_root_)->run(); // Pass doc_root_ here
            return;
        }

        // Launch a plain session if SSL was not detected.
        std::make_shared<plain_http_session>(
                std::move(stream_),
                std::move(buffer_),
                doc_root_)->run(); // Pass doc_root_ here
    }
};

#endif // DETECT_SESSION_HPP

