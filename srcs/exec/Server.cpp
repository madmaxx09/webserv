/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/08 09:59:24 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 14:40:40 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "ServerManager.hpp"
#include "../parser/confParser.hpp"
#include "../request/Request.hpp"
#include "../response/Router.hpp"
#include "../response/Response.hpp"
#include "../webserv.hpp"
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

#define BUFFER_SIZE 4096

Server::Server(void){}

Server::Server(t_server_scope config) : _config(config){

	_still_alive = true;
	//std::cout << "Server created" << std::endl;
}

Server::~Server(void){

	//std::cout << "Server destroyed" << std::endl;
	_requests.clear();
}

int	Server::createServerSocket(void){

	int	rc;

	this->_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_fd < 0)
		std::cerr << "ERROR: Socket() failed" << std::endl;

	int reuse = 1;
	rc = setsockopt(this->_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (rc < 0)
	{
		std::cerr << "ERROR: Setsockopt() failed" << std::endl;
		return (-1);
	}

	std::memset((char *)&_server_addr, 0, sizeof(_server_addr));
	_server_addr.sin_family = AF_INET;
	_server_addr.sin_addr.s_addr = INADDR_ANY;
	_server_addr.sin_port = htons(this->_config.port);

	rc = bind(this->_fd, (struct sockaddr *) &(_server_addr), sizeof(_server_addr));
	if (rc < 0)
	{
		std::cerr << "ERROR: Bind() failed" << std::endl;
		return (-1);
	}
	int	connection_backlog = 1000;
	rc = listen(this->_fd, connection_backlog);
	if (rc < 0)
	{
		std::cerr << "ERROR: Listen() failed" << std::endl;
		return (-1);
	}
	return (this->_fd);
}

int	Server::listenClientConnection(void){

	int			flags;
	int			client_fd;
	socklen_t	client_addr_len = sizeof(_client_addr);

	client_fd = accept(this->_fd, (struct sockaddr *) &(_client_addr), &client_addr_len);

	if (client_fd < 0)
	{
		std::cerr << "ERROR: Accept() failed" << std::endl;
		return (-1);
	}
	_requests.insert(std::make_pair(client_fd, ""));
	flags = fcntl(client_fd, F_SETFL, O_NONBLOCK);
	if (flags < 0)
	{
		std::cerr << "ERROR: Fcntl() failed" << std::endl;
		return (-1);
	}
	return (client_fd);
}

int	Server::readClientSocket(int client_fd){

	int		rc;
	char	buffer[BUFFER_SIZE] = {0};

	rc = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
	if (rc == 0 || rc == -1)
	{
		if (!rc)
			return (0);
		else
		{
			this->closeClientSocket(client_fd);
			std::cerr << "ERROR: Recv() failed" << std::endl;
		}
		return (-1);
	}
	buffer[rc] = '\0';
	_requests[client_fd].append(buffer, rc);
	// Check for chunked requests
	size_t i = _requests[client_fd].find("\r\n\r\n");
	if (i != std::string::npos) // If the end of the headers is found, then check if there is a chunked or a content length
	{
		if(_requests[client_fd].find("Transfer-Encoding: chunked") != std::string::npos)
		{
			if (_requests[client_fd].find("0\r\n\r\n") != std::string::npos)
				return (0); // The complete request has been received
			else
				return (1); // The request is not yet complete / Problem with the position of line breaks (\r\n after each header, double \r\n at the end of the headers, and \r\n at the end of the body).
		}
		size_t pos = _requests[client_fd].find("Content-Length: ");
		if (pos != std::string::npos)
		{
			size_t end_of_line = _requests[client_fd].find("\r\n", pos);
			if (end_of_line != std::string::npos)
			{
				// Extract the value of Content-Length
				size_t length_start = pos + 16; // "Content-Length: " is 16 characters long
				size_t content_length = ft_atoi(_requests[client_fd].substr(length_start, end_of_line - length_start).c_str());
				if (content_length < 0 || content_length > 30000000)
				{
					_requests.erase(client_fd);
					this->closeClientSocket(client_fd);
					std::cerr << "ERROR: Wrong content length, file too large" << std::endl;
					return (-1);
				}
				// Check if the entire request has been received
				if (_requests[client_fd].size() >= i + content_length + 4)
					return (0); // The complete request has been received
				else
					return (1); // The request is not yet complete / Problem with the position of line breaks (\r\n after each header, double \r\n at the end of the headers, and \r\n at the end of the body).
			}
		}
		return (0); // Content-Length or chunked header not found
	}
	// All headers are not present because \r\n\r\n was not found / Problem with the position of line breaks (\r\n after each header, double \r\n at the end of the headers, and \r\n at the end of the body).
	return (1);
}

int	Server::handleRequest(int client_fd){

	t_locations	loc;
	Router		router;
	Response	response;

	//std::cout << _requests[client_fd] << std::endl;
	Request request(_requests[client_fd], *this);
	response.setVersion(request.getVersion());
	if (request.getPathToFile().find("/favicon.ico") != std::string::npos)
		response.errorResponse(404, "Not Found", getConfig().error_page_paths);
	else if (request.getErrorCode() == -1)
	{
		if (checkServerAvailability(request) == false)
		{
			std::string path_to_file = request.getPathToFile();

			if (router.routeRequest(path_to_file, loc, this->_config.locations, response) == true)
			{
				// If redir, generate a redir response and the browser will resend a GET request with the new direction
				if (response.getRedir() == true)
					response.generateResponse();
				else
				{
					request.setPathToFile(path_to_file);

					response.handleDirective(request.getPathToFile(), loc, request, *this);
					if (response.getDefaultFile() == true)
					{
						// When a default file is found we relaunch all the routine, router and response process
						std::cout << "Default file detected, relaunch response processing" << std::endl;
						response.setDefaultFile(false);
						response.handleDirective(request.getPathToFile(), loc, request, *this);
					}
					else if (response.getCGI() == true)
					{
						// If CGI is runned in the response process, we do not have to send a response in the client socket because script already done it
						_requests.erase(client_fd);
						this->closeClientSocket(client_fd);
						return (1);
					}
				}
			}
			else
				response.errorResponse(500, "Internal Server Error", getConfig().error_page_paths);
		}
	}
	else
	{
		std::cout << "Error detected by parsing" << std::endl;
		// Catch error from the request parser
		response.errorResponse(request.getErrorCode(), request.getErrorMsg(), getConfig().error_page_paths);
	}

	_requests.erase(client_fd);
	_requests.insert(std::make_pair(client_fd, response.getResponse()));

	return (0);
}

int	Server::sendResponse(int client_fd){

	int	rc;
	int	len;

	//std::cout << _requests[client_fd] << std::endl;
	len = _requests[client_fd].length();
	rc = send(client_fd, _requests[client_fd].c_str(), len, 0);
	if (rc == 0 || rc == -1)
	{
		_requests.erase(client_fd);
		this->closeClientSocket(client_fd);
		if (!rc)
		{
			std::cout << "Client close connection" << std::endl;
			return (0);
		}
		else
		{
			std::cerr << "ERROR: Send() failed" << std::endl;
			return (-1);
		}
	}
	else if (rc != static_cast<int>(_requests[client_fd].length()))
	{
		std::string	still_to_send;

		std::cout << "Still content to send" << std::endl;
		still_to_send = _requests[client_fd].substr(rc, len);
		_requests.erase(client_fd);
		_requests.insert(std::make_pair(client_fd, still_to_send));
		return (1);
	}
	else
	{
		_requests.erase(client_fd);
		this->closeClientSocket(client_fd);
		std::cout << "Response to client sent" << std::endl;
		return (0);
	}
}

bool	Server::checkServerAvailability(Request& req){

	if (req.getRequestMethod() == "GET" && req.getPathToFile() == "/exit")
	{
		std::cout << "Server off" << std::endl;
		_still_alive = false;
		return (true);
	}
	return (false);
}

void	Server::closeServerSocket(void){

	if (_fd > 0)
		close(_fd);
	_fd = -1;
}

void	Server::closeClientSocket(int client_fd){

	if (client_fd > 0)
		close(client_fd);
	_requests.erase(client_fd);
}

int	Server::getFd(void) const{

	return _fd;
}

int	Server::getClientFd(void) const
{
	return (this->_requests.begin()->first);
}

t_server_scope	Server::getConfig() const{

	return _config;
}

struct sockaddr_in	Server::getClientAddr() const{

	return _client_addr;
}

std::map<int, std::string>	Server::getRequests(void) const{

	return _requests;
}

bool	Server::getStillAlive(void) const{

	return _still_alive;
}

void	Server::setRequests(std::map<int, std::string> requests){

	_requests = requests;
}
