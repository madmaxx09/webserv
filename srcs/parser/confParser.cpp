/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   confParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/06/25 12:15:36 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 14:58:55 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "confParser.hpp"

static void		freeConfig(t_server_scope *serverConfig, int servers)
{
	(void)servers;
	delete[] serverConfig;
}

static bool		isServer(int i, std::string buffer)
{
	if (buffer.substr(i, 6) != "server" || buffer[i + 6] != '\n' || buffer[i + 7] != '{' || buffer[i + 8] != '\n')
	{
		std::cerr << "ERROR: Wrong syntax at 'server' line." << std::endl;
		return (false);
	}
	return (true);
}

static t_server_scope *isServerName(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 13) != "server_name\t\t")
	{
		freeConfig(serverConfig, *servers);
		return (NULL);
	}
	*i += 12;
	std::vector<std::string> names;
	while (buffer[*i] && buffer[*i] != '\n')
	{
		*i += 1;
		int j = *i;
		while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
			j++;
		if (j != *i && buffer[j] && (buffer[j] == '\n' || buffer[j] == ' '))
		{
			names.push_back(buffer.substr(*i, j - *i));
			*i = j;
		}
		else
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
	}
	*i += 1;
	serverConfig[*servers].server_name = names;
	return (serverConfig);
}

static t_server_scope *isServerErrPage(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 12) != "error_page\t\t")
	{
		freeConfig(serverConfig, *servers);
		return (NULL);
	}
	*i += 11;
	std::map<int, std::string> result;
	while (buffer[*i] && buffer[*i] != '\n')
	{
		*i += 1;
		int j = *i;
		while (buffer[j] && std::isdigit(buffer[j]))
			j++;
		int error_code = ft_atoi(buffer.substr(*i, j - *i).c_str());
		if (j != *i && buffer[j] && buffer[j] == ' ' && error_code >= 400 && error_code < 600)
		{
			j++;
			*i = j;
			while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
				j++;
			if (buffer[j] && (buffer[j] == ' ' || buffer[j] == '\n'))
			{
				result[error_code] = buffer.substr(*i, j - *i);
				*i = j;
			}
			else
			{
				freeConfig(serverConfig, *servers);
				return (NULL);
			}
		}
		else
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
	}
	*i += 1;
	serverConfig[*servers].error_page_paths = result;
	return (serverConfig);
}

static t_server_scope	*isServerHost(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 9) == "listen\t\t\t")
	{
		*i += 9;
		int j =	*i;
		while (std::isdigit(buffer[j]))
			j++;
		if (j == *i)
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		int res = ft_atoi(buffer.substr(*i, j - *i).c_str());
		*i = j;
		if (buffer[*i] != '\n' || res < 0 || res > 65353)
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		serverConfig[*servers].port = res;
		*i += 1;
		return (serverConfig);
	}
	freeConfig(serverConfig, *servers);
	return (NULL);
}

static t_server_scope *isServerUploadPath(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 13) != "upload_path\t\t")
	{
		freeConfig(serverConfig, *servers);
		return (NULL);
	}
	*i += 13;
	int j = *i;
	while (buffer[j] && buffer[j] != '\n' && buffer[j] != ' ' && std::isprint(buffer[j]))
		j++;
	if (j != *i && buffer[j] && buffer[j] == '\n')
	{
		serverConfig[*servers].upload_path = buffer.substr(*i, j - *i);
		*i = j + 1;
		return (serverConfig);
	}
	freeConfig(serverConfig, *servers);
	return (NULL);
}

static t_server_scope	*isServerCGI(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 6) == "CGI\t\t\t")
	{
		*i += 6;
		int j = *i;
		while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
			j++;
		if (j != *i)
		{
			std::map<std::string, std::string> result;
			if (serverConfig[*servers].cgi_path.empty() == 0)
				result = serverConfig[*servers].cgi_path;
			std::string path = buffer.substr(*i, j - *i);
			j += 1;
			*i = j;
			if (buffer[j] == '.')
			{
				while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
					j++;
			}
			if (j != *i && buffer[j] == '\n')
			{
				result[buffer.substr(*i, j - *i)] = path;
				*i = j + 1;
				serverConfig[*servers].cgi_path = result;
				return (serverConfig);
			}
		}
	}
	freeConfig(serverConfig, *servers);
	return (NULL);
}

static t_server_scope	*isServerMaxBodySize(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	if (buffer.substr(*i, 21) == "client_max_body_size\t")
	{
		*i += 21;
		int j =	*i;
		while (std::isdigit(buffer[j]))
			j++;
		if (j == *i)
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		int res = ft_atoi(buffer.substr(*i, j - *i).c_str());
		*i = j;
		if (buffer[*i] != '\n' || res < 0 || res > 30000000)
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		serverConfig[*servers].max_body_size = res;
		*i += 1;
		return (serverConfig);
	}
	freeConfig(serverConfig, *servers);
	return (NULL);
}

static t_locations	*handleA(int *i, std::string buffer, t_locations *res)
{
	if (buffer.substr(*i, 10) == "autoindex ")
	{
		*i += 10;
		if (buffer.substr(*i, 3) == "on\n")
		{
			*i += 3;
			res->auto_index = true;
			return (res);
		}
		else if (buffer.substr(*i, 4) == "off\n")
		{
			*i += 4;
			res->auto_index = false;
			return (res);
		}
		delete res;
		return (nullptr);
	}
	else if (buffer.substr(*i, 15) == "allowed_methods")
	{
		std::map<std::string, bool> result;
		result["GET"] = false;
		result["POST"] = false;
		result["DELETE"] = false;
		*i += 15;
		while (buffer[*i] && buffer[*i] == ' ')
		{
			if (buffer.substr(*i, 4) == " GET")
			{
				*i += 4;
				result["GET"] = true;
			}
			else if (buffer.substr(*i, 5) == " POST")
			{
				*i += 5;
				result["POST"] = true;
			}
			else if (buffer.substr(*i, 7) == " DELETE")
			{
				*i += 7;
				result["DELETE"] = true;
			}
			else
			{
				delete res;
				return (nullptr);
			}
		}
		if (buffer[*i] == 0 || buffer[*i] != '\n' || (result["GET"] == false && result["POST"] == false && result["DELETE"] == false))
		{
			delete res;
			return (nullptr);
		}
		res->allowed_methods = result;
		return (res);
	}
	delete res;
	return (nullptr);
}

static t_locations	*handleR(int *i, std::string buffer, t_locations *res)
{
	if (buffer.substr(*i, 5) == "root ")
	{
		*i += 5;
		int j = *i;
		while (buffer[j] && buffer[j] != '\n' && buffer[j] != ' ' && std::isprint(buffer[j]))
			j++;
		if (j != *i && buffer[j] && buffer[j] == '\n')
		{
			res->root_path = buffer.substr(*i, j - *i);
			*i = j + 1;
			return (res);
		}
		delete res;
		return (nullptr);
	}
	else if (buffer.substr(*i, 12) == "redirections" && buffer[*i + 12] == ' ')
	{
		*i += 12;
		std::map<std::string, std::string> result;
		while (buffer[*i] && buffer[*i] == ' ')
		{
			*i += 1;
			int	j = *i;
			while (buffer[j] && buffer[j] != '\n' && buffer[j] != ' ' && std::isprint(buffer[j]))
				j++;
			std::string	tmp = buffer.substr(*i, j - *i);
			for (size_t k = 0; k < tmp.length(); ++k)
				tmp[k] = std::tolower(tmp[k]);
			if (j != *i && buffer[j] && buffer[j] == ' ' && buffer[j] != '\n')
			{
				j++;
				*i = j;
				while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
					j++;
				if (buffer[j] && (buffer[j] == ' ' || buffer[j] == '\n'))
				{
					result[tmp] = buffer.substr(*i, j - *i);
					for (size_t k = 0; k < result[tmp].length(); ++k)
						result[tmp][k] = std::tolower(result[tmp][k]);
					*i = j;
				}
				else
				{
					delete res;
					return (nullptr);
				}
			}
			else
			{
				delete res;
				return (nullptr);
			}
		}
		if (buffer[*i] && buffer[*i] == '\n')
		{
			res->redirections = result;
			return (res);
		}
		delete res;
		return (nullptr);
	}
	delete res;
	return (nullptr);
}

static t_locations	*handleD(int *i, std::string buffer, t_locations *res)
{
	if (buffer.substr(*i, 7) == "default" && buffer[*i + 7] == ' ')
	{
		*i += 7;
		std::vector<std::string> indexes;
		while (buffer[*i] && buffer[*i] != '\n')
		{
			*i += 1;
			int	j = *i;
			while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
				j++;
			if (j != *i && buffer[j] && (buffer[j] == '\n' || buffer[j] == ' '))
			{
				indexes.insert(indexes.begin(), buffer.substr(*i, j - *i));
				*i = j;
			}
			else
			{
				delete res;
				return (nullptr);
			}
		}
		if (buffer[*i] && buffer[*i] == '\n')
		{
			(*i)++;
			res->default_path = indexes;
			return (res);
		}
	}
	delete res;
	return (nullptr);
}

static t_locations	*initLocation()
{
	t_locations *res = new t_locations;
	if (!res)
		return (nullptr);
	res->root_path = "";
	res->auto_index = false;
	return (res);
}

static t_locations	*getLocationParams(int *i, std::string buffer)
{
	t_locations *res = initLocation();
	if (!res)
		return (nullptr);
	while (buffer[*i] && buffer.substr(*i, 2) != "\t}")
	{
		if (buffer[*i] && buffer.substr(*i, 4) != "\t\t\t\t" && buffer[*i] != '\n')
		{
			delete res;
			return (nullptr);
		}
		if (buffer[*i] == '\n')
		{
			(*i)++;
			continue ;
		}
		*i += 4;
		switch (buffer[*i])
		{
			case 'a' :
				if (!(res = handleA(i, buffer, res)) || res == nullptr)
				{
					delete res;
					return (nullptr);
				}
				break ;
			case 'r' :
				if (!(res = handleR(i, buffer, res)))
				{
					delete res;
					return (nullptr);
				}
				break ;
			case 'd' :
				if (!(res = handleD(i, buffer, res)))
				{
					delete res;
					return (nullptr);
				}
				break ;
			default :
				delete res;
				return (nullptr);
		}
	}
	return (res);
}

static t_server_scope	*isServerLocation(int *i, std::string buffer, t_server_scope *serverConfig, int *servers, int *locs)
{
	if (buffer.substr(*i, 9) == "Location ")
	{
		*i += 9;
		int j = *i;
		while (buffer[j] && std::isprint(buffer[j]) && buffer[j] != ' ')
			j++;
		if (j != *i && buffer.substr(j, 4) == "\n\t{\n")
		{
			std::map<std::string, t_locations> result;
			if (*locs > 0)
				result = serverConfig[*servers].locations;
			std::string location = buffer.substr(*i, j - *i);
			for (size_t k = 0; k < location.length(); ++k)
				location[k] = std::tolower(location[k]);
			*i = j + 4;
			t_locations	*res = nullptr;
			if (!(res = getLocationParams(i, buffer)))
			{
				freeConfig(serverConfig, *servers);
				return (NULL);
			}
			result[location] = *res;
			delete res;
			if (buffer.substr(*i, 2) == "\t}")
			{
				*i += 2;
				serverConfig[*servers].locations = result;
				(*locs)++;
				return (serverConfig);
			}
		}
	}
	freeConfig(serverConfig, *servers);
	return (NULL);
}

static t_server_scope	*getServerConfig(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	int locs = 0;
	//While inside a 'server' block, calling the appropriate function according to the first character.
	serverConfig[*servers].max_body_size = 0;
	serverConfig[*servers].port = 0;
	while (buffer[*i] && buffer[*i] != '}')
	{
		if (buffer[*i] && buffer[*i] != '\t' && buffer[*i] != '\n')
		{
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		if (buffer[*i] && buffer[*i] == '\n')
		{
			*i += 1;
			continue ;
		}
		*i += 1;
		switch (buffer[*i])
		{
			case 's' :
				if (!(serverConfig = isServerName(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'l' :
				if (!(serverConfig = isServerHost(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'e' :
				if (!(serverConfig = isServerErrPage(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'c' :
				if (!(serverConfig = isServerMaxBodySize(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'C' :
				if (!(serverConfig = isServerCGI(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'u' :
				if (!(serverConfig = isServerUploadPath(i, buffer, serverConfig, servers)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case 'L' :
				if (!(serverConfig = isServerLocation(i, buffer, serverConfig, servers, &locs)))
				{
					freeConfig(serverConfig, *servers);
					return (NULL);
				}
				break ;
			case '\n' :
				*i += 1;
				break ;
			default :
				freeConfig(serverConfig, *servers);
				return (NULL);
		}
	}
	if (buffer[*i + 1] && buffer[*i + 1] != '\n')
	{
		freeConfig(serverConfig, *servers);
		return (NULL);
	}
	*i += 1;
	return (serverConfig);
}

static t_server_scope	*parseServer(int *i, std::string buffer, t_server_scope *serverConfig, int *servers)
{
	//Checking for "server" line, passing and reallocating it if it's a new server.
	if (!isServer(*i, buffer))
	{
		freeConfig(serverConfig, *servers);
		return (NULL);
	}
	*servers += 1;
	*i += 9;
	t_server_scope *temp = new t_server_scope[*servers + 1];
	if (*servers > 0) {
		for (int j = 0; j < *servers; ++j) {
			temp[j] = serverConfig[j];
		}
		delete[] serverConfig;
	}
	serverConfig = temp;
	//Checking for each line and calling the appropriate function while we're in the server block.
	if (!(serverConfig = getServerConfig(i, buffer, serverConfig, servers)))
		return (NULL);
	return (serverConfig);
}

static t_server_scope	*checkConfig(t_server_scope *serverConfig, int *servers)
{
	//Loop over every server, loop over every location to check and set defaults values
	//Atleast a port, server_name and 1 location.
	for (int i = 0; i <= *servers; i++)
	{
		std::map<std::string, t_locations>::iterator it = serverConfig[i].locations.begin();
		if (it == serverConfig[i].locations.end() || !serverConfig[i].port || serverConfig[i].upload_path.empty() == 1 || serverConfig[i].server_name[0].empty() == 1)
		{
			std::cerr << "ERROR : configuration file lacks mandatory informations" << std::endl;
			freeConfig(serverConfig, *servers);
			return (NULL);
		}
		if (serverConfig[i].max_body_size <= 0)
			serverConfig[i].max_body_size = 1024;
		while (it != serverConfig[i].locations.end())
		{
			if (it->second.root_path.empty() == 1)
				it->second.root_path = "./var/www/html/";
			if (it->second.allowed_methods.begin() == it->second.allowed_methods.end())
			{
				std::map<std::string, bool> res;
				res["GET"] = false;
				res["POST"] = false;
				res["DELETE"] = false;
				it->second.allowed_methods = res;
			}
			++it;
		}
	}
	return (serverConfig);
}

t_server_scope		*confParser(std::string buffer, int *servers)
{
	t_server_scope	*serverConfig = NULL;
	for (int i = 0; buffer[i] && buffer[i] != 0; i++)
	{
		while (buffer[i] && buffer[i] == '\n')
			i++;
		if (!(serverConfig = parseServer(&i, buffer, serverConfig, servers)))
			return (NULL);
	}
	//Minimal config checker
	if (!(serverConfig = checkConfig(serverConfig, servers)))
		return (NULL);
	return (serverConfig);
}
