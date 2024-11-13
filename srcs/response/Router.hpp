/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/22 11:21:28 by gt-serst          #+#    #+#             */
/*   Updated: 2024/06/26 10:44:57 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "../parser/confParser.hpp"
# include "../response/Response.hpp"
# include <string>
# include <map>
# include <vector>

class Router{

	public:
		Router(void);
		~Router(void);
		bool	routeRequest(std::string& path_to_file, t_locations& loc, std::map<std::string, t_locations> routes, Response& resp);

	private:
		bool		recursiveRouteRequest(std::string tmp, t_locations& loc, std::map<std::string, t_locations> routes);
		std::string	removeLastPath(std::string tmp);
		std::string	handleRedirection(std::string path_to_file, std::map<std::string, std::string> redirections);
};

#endif
