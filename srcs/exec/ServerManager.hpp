/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/07 11:00:48 by gt-serst          #+#    #+#             */
/*   Updated: 2024/06/05 17:22:27 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERMANAGER_HPP
# define SERVERMANAGER_HPP

# include "Server.hpp"
# include "../parser/confParser.hpp"
# include <map>
# include <vector>
# include <sys/select.h>

class ServerManager
{
	public:
		ServerManager(void);
		~ServerManager(void);
		void					launchServer(t_server_scope *servers, int nb_servers);

	private:
		void					initServer(t_server_scope *servers, int nb_servers);
		void					serverRoutine(void);
		std::map<int, Server>	_servers;
		std::map<int, Server*>	_sockets;
		std::vector<int>		_ready;
		fd_set					_fd_set;
		int						_max_fd;
};

#endif
