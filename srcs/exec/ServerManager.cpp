/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/07 11:04:51 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 13:38:15 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerManager.hpp"
#include "Server.hpp"
#include "../parser/confParser.hpp"
#include <cstring>
#include <map>
#include <vector>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <iostream>

ServerManager::ServerManager(void){

	//std::cout << "ServerManager created" << std::endl;
}

ServerManager::~ServerManager(void){

	for (std::map<int, Server>::iterator it = _servers.begin(); it != _servers.end(); ++it)
		it->second.closeServerSocket();
	for (std::map<int, Server*>::iterator it = _sockets.begin(); it != _sockets.end(); ++it)
		it->second->closeClientSocket(it->first);
	_servers.clear();
	_sockets.clear();
	_ready.clear();
	// std::cout << "ServerManager destroyed" << std::endl;
}

void	ServerManager::launchServer(t_server_scope *servers, int nb_servers){

	initServer(servers, nb_servers);
	serverRoutine();
}

void	ServerManager::initServer(t_server_scope *servers, int nb_servers){

	FD_ZERO(&_fd_set);
	this->_max_fd = 0;

	for (int i = 0; i < nb_servers; i++)
	{
		int		fd;
		Server	server(servers[i]);

		// Initialize server fd socket and set them
		if (server.createServerSocket() != -1)
		{
			fd = server.getFd();
			FD_SET(fd, &_fd_set);
			if (fd > this->_max_fd)
				this->_max_fd = fd;
			_servers.insert(std::make_pair(fd, server));
			std::cout << "Server launched" << std::endl;
		}
	}
	std::cout << "All working servers are ready" << std::endl;
}

void	ServerManager::serverRoutine(void){

	while (1)
	{
		int				rc;
		fd_set			reading_set;
		fd_set			writing_set;
		struct timeval	timeout;

		rc = 0;
		std::cout << "Waiting for connection..." << std::endl;
		while (rc == 0)
		{
			// After each loop fds information are kept from the previous loop and select is relaunched
			timeout.tv_sec  = 1;
			timeout.tv_usec = 0;
			std::memcpy(&reading_set, &_fd_set, sizeof(_fd_set));
			FD_ZERO(&writing_set);
			for (std::vector<int>::iterator it = _ready.begin(); it < _ready.end(); ++it)
				FD_SET(*it, &writing_set);

			rc = select(_max_fd + 1, &reading_set, &writing_set, NULL, &timeout);
		}
		if (rc > 0)
		{
			for (std::vector<int>::iterator it = _ready.begin(); rc && it != _ready.end(); ++it)
			{
				// Client socket is set and ready to send the response to the browser
				if (FD_ISSET(*it, &writing_set))
				{
					std::cout << "Send response to client" << std::endl;
					int rc = _sockets[*it]->sendResponse(*it);
					// Chunked response detected
					if (rc != 1)
					{
						FD_CLR(*it, &_fd_set);
						FD_CLR(*it, &reading_set);
						_sockets.erase(*it);
						_ready.erase(it);
					}
					rc = 0;
					break;
				}
			}
			for (std::map<int, Server*>::iterator it = _sockets.begin(); rc && it != _sockets.end(); ++it)
			{
				// Client socket is set and ready to send the request to the server
				if (FD_ISSET(it->first, &reading_set))
				{
					std::cout << "Read client request" << std::endl;
					int rc = it->second->readClientSocket(it->first);
					if (rc == 0)
					{
						std::cout << "Entire request read, start request processing" << std::endl;
						rc = it->second->handleRequest(it->first);
						if (rc == 0)
							_ready.push_back(it->first);
						// CGI detected
						else if (rc == 1)
						{
							FD_CLR(it->first, &_fd_set);
							FD_CLR(it->first, &reading_set);
							_sockets.erase(it->first);
						}
					}
					else if (rc == -1)
					{
						FD_CLR(it->first, &_fd_set);
						FD_CLR(it->first, &reading_set);
						_sockets.erase(it->first);
					}
					rc = 0;
					break;
				}
			}
			for (std::map<int, Server>::iterator it = _servers.begin(); rc && it != _servers.end(); ++it)
			{
				// Server socket is set and ready to listen to a client
				if (FD_ISSET(it->first, &reading_set))
				{
					std::cout << "Get a client connection" << std::endl;
					int client_fd = it->second.listenClientConnection();

					if (client_fd != -1)
					{
						FD_SET(client_fd, &_fd_set);
						_sockets.insert(std::make_pair(client_fd, &(it->second)));
						if (client_fd > this->_max_fd)
							this->_max_fd = client_fd;
					}
					rc = 0;
					break;
				}
			}
		}
		else
		{
			// Select failed because his return code is lower than 0
			for (std::map<int, Server*>::iterator it = _sockets.begin() ; it != _sockets.end() ; it++)
				it->second->closeClientSocket(it->first);
			_sockets.clear();
			_ready.clear();
			FD_ZERO(&_fd_set);
			for (std::map<int, Server>::iterator it = _servers.begin() ; it != _servers.end() ; it++)
				FD_SET(it->first, &_fd_set);
		}
		for (std::map<int, Server>::iterator it = _servers.begin(); it != _servers.end(); ++it)
		{
			if (it->second.getStillAlive() == false)
			{
				for (std::map<int, Server*>::iterator it = _sockets.begin(); it != _sockets.end(); ++it)
				{
					FD_CLR(it->first, &_fd_set);
					FD_CLR(it->first, &reading_set);
				}
				return;
			}
		}
	}
}
