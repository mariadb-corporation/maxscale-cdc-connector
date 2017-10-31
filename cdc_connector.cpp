/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2020-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "cdc_connector.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <jansson.h>
#include <iostream>

#define CDC_CONNECTOR_VERSION "1.0.0"

#define ERRBUF_SIZE 512
#define READBUF_SIZE 1024

static const char OK_RESPONSE[] = "OK\n";

static const char CLOSE_MSG[] = "CLOSE";
static const char REGISTER_MSG[] = "REGISTER UUID=CDC_CONNECTOR-" CDC_CONNECTOR_VERSION ", TYPE=";
static const char REQUEST_MSG[] = "REQUEST-DATA ";

namespace
{

static inline void millisleep(int millis)
{
    struct timespec ts = {0, 1000000};
    nanosleep(&ts, NULL);
}

static std::string bin2hex(const uint8_t *data, size_t len)
{
    std::string result;
    static const char hexconvtab[] = "0123456789abcdef";

    for (int i = 0; i < len; i++)
    {
        result += hexconvtab[data[i] >> 4];
        result += hexconvtab[data[i] & 0x0f];
    }

    return result;
}

std::string generateAuthString(const std::string& user, const std::string& password)
{
    uint8_t digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const uint8_t*> (password.c_str()), password.length(), digest);

    std::string auth_str = user;
    auth_str += ":";

    std::string part1 = bin2hex((uint8_t*)auth_str.c_str(), auth_str.length());
    std::string part2 = bin2hex(digest, sizeof(digest));

    return part1 + part2;
}


std::string json_to_string(json_t* json)
{
    std::stringstream ss;

    switch (json_typeof(json))
    {
    case JSON_STRING:
        ss << json_string_value(json);
        break;

    case JSON_INTEGER:
        ss << json_integer_value(json);
        break;

    case JSON_REAL:
        ss << json_real_value(json);
        break;

    case JSON_TRUE:
        ss << "true";
        break;

    case JSON_FALSE:
        ss << "false";
        break;

    case JSON_NULL:
        break;

    default:
        break;

    }

    return ss.str();
}

}

namespace CDC
{

/**
 * Public functions
 */

Connection::Connection(const std::string& address,
                       uint16_t port,
                       const std::string& user,
                       const std::string& password,
                       int timeout) :
    m_fd(-1),
    m_address(address),
    m_port(port),
    m_user(user),
    m_password(password),
    m_timeout(timeout)
{
}

Connection::~Connection()
{
    closeConnection();
}

bool Connection::createConnection()
{
    bool rval = false;
    struct sockaddr_in remote = {};

    remote.sin_port = htons(m_port);
    remote.sin_family = AF_INET;

    if (inet_aton(m_address.c_str(), (struct in_addr*)&remote.sin_addr.s_addr) == 0)
    {
        m_error = "Invalid address: ";
        m_error += m_address;
    }
    else
    {
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (fd == -1)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to create socket: ";
            m_error += strerror_r(errno, err, sizeof(err));
        }

        m_fd = fd;
        int fl;

        if (connect(fd, (struct sockaddr*) &remote, sizeof(remote)) == -1)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to connect: ";
            m_error += strerror_r(errno, err, sizeof(err));
        }
        else if ((fl = fcntl(fd, F_GETFL, 0)) == -1 ||
                 fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to set socket non-blocking: ";
            m_error += strerror_r(errno, err, sizeof(err));
        }
        else if (doAuth())
        {
            rval = doRegistration();
        }
    }

    return rval;
}

void Connection::closeConnection()
{
    if (m_fd != -1)
    {
        nointr_write(CLOSE_MSG, sizeof(CLOSE_MSG) - 1);
        close(m_fd);
        m_fd = -1;
    }
}

bool Connection::requestData(const std::string& table, const std::string& gtid)
{
    bool rval = true;

    std::string req_msg(REQUEST_MSG);
    req_msg += table;

    if (gtid.length())
    {
        req_msg += " ";
        req_msg += gtid;
    }

    if (nointr_write(req_msg.c_str(), req_msg.length()) == -1)
    {
        rval = false;
        char err[ERRBUF_SIZE];
        m_error = "Failed to write request: ";
        m_error += strerror_r(errno, err, sizeof(err));
    }

    return rval;
}

static inline bool is_schema(json_t* json)
{
    bool rval = false;
    json_t* j = json_object_get(json, "fields");

    if (j && json_is_array(j) && json_array_size(j))
    {
        rval = json_object_get(json_array_get(j, 0), "name") != NULL;
    }

    return rval;
}

void Connection::processSchema(json_t* json)
{
    m_keys.clear();
    m_types.clear();

    json_t* arr = json_object_get(json, "fields");
    char* raw = json_dumps(json, 0);
    size_t i;
    json_t* v;

    json_array_foreach(arr, i, v)
    {
        json_t* name = json_object_get(v, "name");
        json_t* type = json_object_get(v, "real_type");
        if (type == NULL)
        {
            // Use the Avro type for generated columns
            type = json_object_get(v, "type");
        }
        std::string nameval = name ? json_string_value(name) : "";
        std::string typeval = type ? (json_is_string(type) ? json_string_value(type) : "char(50)") : "undefined";

        m_keys.push_back(nameval);
        m_types.push_back(typeval);
    }
}

Row Connection::processRow(json_t* js)
{
    ValueList values;
    m_error.clear();

    for (ValueList::iterator it = m_keys.begin();
         it != m_keys.end(); it++)
    {
        json_t* v = json_object_get(js, it->c_str());

        if (v)
        {
            values.push_back(json_to_string(v));
        }
        else
        {
            m_error = "No value for key found: ";
            m_error += *it;
            break;
        }
    }

    Row rval;

    if (m_error.empty())
    {
        rval = Row(new InternalRow(m_keys, m_types, values));
    }

    return rval;
}

Row Connection::read()
{
    Row rval;
    std::string row;

    if (readRow(row))
    {
        json_error_t err;
        json_t* js = json_loads(row.c_str(), JSON_ALLOW_NUL, &err);

        if (js)
        {
            if (is_schema(js))
            {
                m_schema = row;
                processSchema(js);
                rval = Connection::read();
            }
            else
            {
                rval = processRow(js);
            }

            json_decref(js);
        }
        else
        {
            m_error = "Failed to parse JSON: ";
            m_error += err.text;
        }
    }

    return rval;
}

/**
 * Private functions
 */

bool Connection::doAuth()
{
    bool rval = false;
    std::string auth_str = generateAuthString(m_user, m_password);

    /** Send the auth string */
    if (nointr_write(auth_str.c_str(), auth_str.length()) == -1)
    {
        char err[ERRBUF_SIZE];
        m_error = "Failed to write authentication data: ";
        m_error += strerror_r(errno, err, sizeof(err));
    }
    else
    {
        /** Read the response */
        char buf[READBUF_SIZE];
        int bytes;

        if ((bytes = nointr_read(buf, sizeof(buf))) == -1)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to read authentication response: ";
            m_error += strerror_r(errno, err, sizeof(err));
        }
        else if (memcmp(buf, OK_RESPONSE, sizeof(OK_RESPONSE) - 1) != 0)
        {
            buf[bytes] = '\0';
            m_error = "Authentication failed: ";
            m_error += buf;
        }
        else
        {
            rval = true;
        }
    }

    return rval;
}

bool Connection::doRegistration()
{
    bool rval = false;
    std::string reg_msg(REGISTER_MSG);
    reg_msg += "JSON";

    /** Send the registration message */
    if (nointr_write(reg_msg.c_str(), reg_msg.length()) == -1)
    {
        char err[ERRBUF_SIZE];
        m_error = "Failed to write registration message: ";
        m_error += strerror_r(errno, err, sizeof(err));
    }
    else
    {
        /** Read the response */
        char buf[READBUF_SIZE];
        int bytes;

        if ((bytes = nointr_read(buf, sizeof(buf))) == -1)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to read registration response: ";
            m_error += strerror_r(errno, err, sizeof(err));
        }
        else if (memcmp(buf, OK_RESPONSE, sizeof(OK_RESPONSE) - 1) != 0)
        {
            buf[bytes] = '\0';
            m_error = "Registration failed: ";
            m_error += buf;
        }
        else
        {
            rval = true;
        }
    }

    return rval;
}

bool Connection::readRow(std::string& dest)
{
    bool rval = true;

    while (true)
    {
        char buf;
        int rc = nointr_read(&buf, 1);

        if (rc == -1)
        {
            rval = false;
            char err[ERRBUF_SIZE];
            m_error = "Failed to read row: ";
            m_error += strerror_r(errno, err, sizeof(err));
            break;
        }
        else if (rc == 0)
        {
            rval = false;
            m_error = "Request timed out";
            break;
        }

        if (buf == '\n')
        {
            break;
        }
        else
        {
            dest += buf;

            if (dest[0] == 'E' && dest[1] == 'R' & dest[2] == 'R')
            {
                m_error = "Server responded with an error: ";
                m_error += dest;
                rval = false;
                break;
            }
        }
    }

    return rval;
}

#define is_poll_error(e) ((e & (POLLERR | POLLHUP | POLLNVAL)))

static std::string event_to_string(int event)
{
    std::string rval;

    if (event & POLLIN)
    {
        rval += "POLLIN ";
    }
    if (event & POLLPRI)
    {
        rval += "POLLPRI ";
    }
    if (event & POLLOUT)
    {
        rval += "POLLOUT ";
    }
#ifdef POLLRDHUP
    if (event & POLLRDHUP)
    {
        rval += "POLLRDHUP ";
    }
#endif
    if (event & POLLERR)
    {
        rval += "POLLERR ";
    }
    if (event & POLLHUP)
    {
        rval += "POLLHUP ";
    }
    if (event & POLLNVAL)
    {
        rval += "POLLNVAL ";
    }

    return rval;
}

int Connection::wait_for_event(short events)
{
    nfds_t nfds = 1;
    struct pollfd pfd;
    pfd.fd = m_fd;
    pfd.events = events;
    int rc = poll(&pfd, nfds, m_timeout * 1000);

    if (rc > 0 && is_poll_error(pfd.revents))
    {
        rc = -1;
        m_error += "Error when waiting event; ";
        m_error += event_to_string(pfd.revents);
    }
    else if (rc < 0)
    {
        char err[ERRBUF_SIZE];
        m_error = "Failed to wait for event: ";
        m_error += strerror_r(errno, err, sizeof(err));
    }

    return rc;
}

int Connection::nointr_read(void *dest, size_t size)
{
    int n_bytes = 0;

    if (wait_for_event(POLLIN) > 0)
    {
        int rc = 0;

        while ((rc = ::read(m_fd, dest, size)) < 0 && errno == EINTR)
        {
            ;
        }

        if (rc == -1 && errno != EWOULDBLOCK && errno != EAGAIN)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to read data: ";
            m_error += strerror_r(errno, err, sizeof(err));
            n_bytes = -1;
        }
        else if (rc > 0)
        {
            n_bytes += rc;
        }
    }

    return n_bytes;
}

int Connection::nointr_write(const void *src, size_t size)
{
    int rc = 0;
    int n_bytes = 0;

    if (wait_for_event(POLLOUT) > 0)
    {
        while ((rc = ::write(m_fd, src, size)) < 0 && errno == EINTR)
        {
            ;
        }

        if (rc < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        {
            char err[ERRBUF_SIZE];
            m_error = "Failed to write data: ";
            m_error += strerror_r(errno, err, sizeof(err));
            n_bytes = -1;
        }
        else if (rc > 0)
        {
            n_bytes += rc;
        }
    }

    return n_bytes;
}

}
